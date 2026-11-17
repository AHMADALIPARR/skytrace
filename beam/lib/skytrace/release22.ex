defmodule Skytrace.Release22 do
  @moduledoc """
  Dedicated bounded-worker protocol for SKYTRACE 2.2's numerical kernel.
  Protocol request: %{"operation" => "physical_fit", "seed" => integer,
                      "job_id" => string}; one NDJSON line yields one response.
  No HTTP fetch or archive mutation occurs in this worker.
  """
  @max_concurrency 2
  @timeout 60_000

  def process_jobs(supervisor, jobs, python, script) when is_list(jobs) do
    Task.Supervisor.async_stream_nolink(supervisor, jobs,
      fn job -> run_job(job, python, script) end,
      ordered: true, max_concurrency: @max_concurrency,
      timeout: @timeout, on_timeout: :kill_task
    )
    |> Enum.map(fn
      {:ok, result} -> result
      {:exit, reason} -> {:error, {:worker_exit, inspect(reason)}}
    end)
  end

  def run_job(job, python, script) when is_map(job) do
    case System.cmd(python, ["-u", script],
      input: :json.encode(job) <> "\n", stderr_to_stdout: false) do
      {out, 0} ->
        case String.split(String.trim(out), "\n") do
          [line] ->
            case :json.decode(line) do
              %{"status" => "ok"} = value -> {:ok, value}
              %{"status" => "error"} = value -> {:error, value}
              _ -> {:error, :bad_reply}
            end
          _ -> {:error, :protocol_violation}
        end
      {_out, exit_code} -> {:error, {:kernel_exit, exit_code}}
    end
  end
end
