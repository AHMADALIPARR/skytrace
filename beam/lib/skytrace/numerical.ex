# SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-SnapKitty-Opaque-1.0
# Copyright (C) 2026 Ahmad Ali Parr / SnapKitty Research Lab
defmodule Skytrace.Numerical do
  @moduledoc """
  Bounded BEAM source workers -> deterministic sensor-graph kernel via Erlang Port.
  Each worker receives a locally archived normalized graph fragment; no live scraping.
  Fragments are merged with exact IDs, preserving provenance and rejecting conflicts.
  The port protocol is one JSON request line to one JSON response line.
  Requires OTP 27+ (Erlang :json) and the skytrace binary (`skytrace analyze`).
  The binary is System.get_env("SKYTRACE"), System.find_executable("skytrace"),
  or an explicit argument.
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
  def analyze(paths, kernel_arg \\ "analyze", skytrace_bin \\ default_skytrace_bin()) do
    results = load_fragments(paths)
    case Enum.find(results, fn x -> match?({:error, _}, x) end) do
      {:error, reason} -> {:error, {:worker_failed, reason}}
      nil ->
        fragments = Enum.map(results, fn {:ok, x} -> x["fragment"] end)
        case merge(fragments) do
          {:ok, graph} -> run_port(graph, kernel_arg, skytrace_bin)
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

  def run_port(_graph, _kernel_arg, nil), do: {:error, :skytrace_missing}
  def run_port(graph, kernel_arg, skytrace_bin) when is_binary(skytrace_bin) do
    # Explicit executable + arg list, no shell interpolation. System.cmd closes
    # stdin after the request so `skytrace analyze` (which reads until EOF) replies.
    task = Task.async(fn ->
      System.cmd(skytrace_bin, [kernel_arg],
        input: JasonSafe.encode!(graph) <> "\n",
        stderr_to_stdout: false
      )
    end)
    case Task.yield(task, 30_000) || Task.shutdown(task, :brutal_kill) do
      {:ok, {out, 0}} ->
        case String.split(out, "\n", trim: true) do
          [line] ->
            case JasonSafe.decode!(line) do
              %{"status" => "ok"} = data -> {:ok, data}
              %{"status" => "error"} = data -> {:error, {:kernel_rejected, data}}
              _ -> {:error, :unexpected_port_reply}
            end
          _ -> {:error, :unexpected_port_reply}
        end
      {:ok, {_out, code}} -> {:error, {:kernel_exit, code}}
      nil -> {:error, :kernel_timeout}
    end
  end

  defp default_skytrace_bin do
    System.get_env("SKYTRACE") || System.find_executable("skytrace")
  end
end
