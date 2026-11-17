# SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-SnapKitty-Opaque-1.0
# Copyright (C) 2026 Ahmad Ali Parr / SnapKitty Research Lab
defmodule Skytrace.Ingest do
  @moduledoc "Bounded supervised snapshot ingestion, with a strict NDJSON OCaml Port boundary."
  @sources ~w(pursue nara aaro)

  @doc "Read a locally archived source-specific JSON/NDJSON snapshot; no hidden live fetch."
  def snapshot!(source, path) when source in @sources and is_binary(path) do
    bytes = File.read!(path)
    digest = :crypto.hash(:sha256, bytes) |> Base.encode16(case: :lower)
    records =
      bytes
      |> String.split("\n", trim: true)
      |> Enum.flat_map(fn line ->
        case JasonSafe.decode!(line) do
          %{"records" => list} when is_list(list) -> list
          row when is_map(row) -> [row]
          _ -> raise ArgumentError, "invalid snapshot JSON"
        end
      end)
    %{source: source, sha256: digest, records: records}
  end

  @doc "Fan out source snapshots, isolate worker exits, preserve deterministic source order."
  def fanout(paths, max_concurrency \\ 3) when is_list(paths) do
    Task.Supervisor.async_stream_nolink(Skytrace.TaskSupervisor, paths, fn {source, path} ->
      snapshot!(source, path)
    end, max_concurrency: max_concurrency, ordered: true, timeout: 120_000, on_timeout: :kill_task)
    |> Enum.map(fn
      {:ok, result} -> {:ok, result}
      {:exit, reason} -> {:error, inspect(reason)}
    end)
  end

  @doc "Send one complete forest snapshot to the OCaml process and read one reply."
  def normalize_port(records, bin) when is_list(records) and is_binary(bin) do
    port = Port.open({:spawn_executable, bin}, [:binary, :exit_status, {:line, 4_194_304}])
    payload = JasonSafe.encode!(%{"records" => records}) <> "\n"
    true = Port.command(port, payload)
    result = receive do
      {^port, {:data, {:eol, line}}} -> JasonSafe.decode!(line)
      {^port, {:data, {:noeol, _}}} -> {:error, :line_too_long}
      {^port, {:exit_status, code}} -> {:error, {:exit, code}}
    after 30_000 -> {:error, :timeout} end
    Port.close(port)
    result
  end
end

# JSON dependency-free boundary implemented with Erlang's `json` module where
# present (OTP 27+). Intentionally fails early otherwise, not a silent fallback.
defmodule JasonSafe do
  def decode!(json) do
    unless Code.ensure_loaded?(:json), do: raise("OTP 27+ :json required for JSON port")
    :json.decode(json)
  end
  def encode!(obj) do
    unless Code.ensure_loaded?(:json), do: raise("OTP 27+ :json required for JSON port")
    :json.encode(obj)
  end
end
