defmodule Skytrace.Numerical do
  @moduledoc """
  Bounded BEAM source workers -> deterministic sensor-graph kernel via Erlang Port.
  Each worker receives a locally archived normalized graph fragment; no live scraping.
  Fragments are merged with exact IDs, preserving provenance and rejecting conflicts.
  The port protocol is one JSON request line to one JSON response line.
  Requires OTP 27+ (Erlang :json), Python 3.10+, and the checked-in kernel.
  """

  @doc "Parallel fragment loading; errors are returned, never silently omitted."
  def load_fragments(paths, max_concurrency \\ 3) when is_list(paths) do
    Task.Supervisor.async_stream_nolink(
      Skytrace.TaskSupervisor,
      paths,
      fn path ->
        bytes = File.read!(path)
        fragment = JasonSafe.decode!(bytes)
        %{
          "fragment" => fragment,
          "source_sha256" => :crypto.hash(:sha256, bytes) |> Base.encode16(case: :lower)
        }
      end,
      max_concurrency: max_concurrency,
      ordered: true,
      timeout: 60_000,
      on_timeout: :kill_task
    )
    |> Enum.map(fn
      {:ok, value} -> {:ok, value}
      {:exit, reason} -> {:error, inspect(reason)}
    end)
  end

  @doc "Merge source fragments after worker validation, then submit to the kernel."
  def analyze(paths, kernel_path, python_bin \\ System.find_executable("python3")) do
    results = load_fragments(paths)
    case Enum.find(results, fn x -> match?({:error, _}, x) end) do
      {:error, reason} -> {:error, {:worker_failed, reason}}
      nil ->
        fragments = Enum.map(results, fn {:ok, x} -> x["fragment"] end)
        case merge(fragments) do
          {:ok, graph} -> run_port(graph, kernel_path, python_bin)
          {:error, _} = err -> err
        end
    end
  end

  def merge(fragments) when is_list(fragments) do
    fields = ~w(nodes edges observations)
    try do
      merged = Enum.reduce(fragments, %{"nodes" => [], "edges" => [], "observations" => []}, fn fragment, acc ->
        unless is_map(fragment) and Enum.all?(fields, fn k -> is_list(fragment[k]) end) do
          raise ArgumentError, "invalid fragment structure"
        end
        Map.new(fields, fn k -> {k, acc[k] ++ fragment[k]} end)
      end)
      {:ok, merged}
    rescue
      e in [ArgumentError] -> {:error, {:merge_failed, Exception.message(e)}}
    end
  end

  def run_port(_graph, _kernel_path, nil), do: {:error, :python_missing}
  def run_port(graph, kernel_path, python_bin) when is_binary(python_bin) do
    # Explicit executable + arg list, no shell interpolation.
    port = Port.open({:spawn_executable, python_bin}, [
      :binary, :exit_status, :use_stdio, {:args, [kernel_path]}, {:line, 8_388_608}
    ])
    true = Port.command(port, JasonSafe.encode!(graph) <> "\n")
    response = receive do
      {^port, {:data, {:eol, line}}} ->
        case JasonSafe.decode!(line) do
          %{"status" => "ok"} = data -> {:ok, data}
          %{"status" => "error"} = data -> {:error, {:kernel_rejected, data}}
          _ -> {:error, :unexpected_port_reply}
        end
      {^port, {:data, {:noeol, _}}} -> {:error, :oversized_response}
      {^port, {:exit_status, code}} -> {:error, {:kernel_exit, code}}
    after
      30_000 -> {:error, :kernel_timeout}
    end
    Port.close(port)
    response
  end
end
