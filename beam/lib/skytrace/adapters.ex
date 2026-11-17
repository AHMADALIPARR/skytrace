defmodule Skytrace.Adapters do
  @moduledoc "Source-specific coercion of *archived structured rows* to normalized input."
  @mime %{"pdf" => "application/pdf", "vid" => "video/mp4", "img" => "image/jpeg", "aud" => "audio/mpeg"}

  def adapt!("pursue", row) when is_map(row) do
    id = required!(row, ["Record ID", "record_id", "id"])
    %{
      "source" => "pursue", "agency" => required!(row, ["Agency", "agency"]),
      "canonical_id" => id, "release_epoch" => required!(row, ["Release Date", "release_date"]),
      "mime" => mime!(row), "has_range_telemetry" => bool_unknown(row, "has_range_telemetry"),
      "corroborated" => bool_unknown(row, "corroborated"), "resolved" => bool_unknown(row, "resolved")
    }
  end
  def adapt!("nara", row) when is_map(row) do
    naid = required!(row, ["naId", "naid", "NAID"])
    %{"source" => "nara", "agency" => Map.get(row, "agency", "NARA"),
      "canonical_id" => "NAID-" <> to_string(naid),
      "release_epoch" => required!(row, ["release_epoch", "publicationDate"]),
      "mime" => Map.get(row, "mime", "application/json"),
      "has_range_telemetry" => bool_unknown(row, "has_range_telemetry"),
      "corroborated" => bool_unknown(row, "corroborated"), "resolved" => bool_unknown(row, "resolved")}
  end
  def adapt!("aaro", row) when is_map(row) do
    %{"source" => "aaro", "agency" => "AARO",
      "canonical_id" => required!(row, ["case_id", "pr_id", "id"]),
      "release_epoch" => required!(row, ["release_date", "published_at"]),
      "mime" => mime!(row), "has_range_telemetry" => bool_unknown(row, "has_range_telemetry"),
      "corroborated" => bool_unknown(row, "corroborated"), "resolved" => case Map.get(row, "resolution_status") do
        "resolved" -> true
        "unresolved" -> false
        _ -> nil
      end}
  end
  def adapt!(source, _), do: raise(ArgumentError, "unsupported source or row: #{inspect(source)}")

  defp required!(row, names) do
    case Enum.find_value(names, fn name ->
      v = Map.get(row, name)
      if (is_binary(v) and String.trim(v) != "") or is_integer(v), do: to_string(v), else: nil
    end) do
      nil -> raise ArgumentError, "missing source metadata field (#{Enum.join(names, "/")})"
      v -> v
    end
  end
  defp bool_unknown(row, field) do
    case Map.get(row, field) do
      true -> true
      false -> false
      nil -> nil
      v -> raise ArgumentError, "invalid #{field}: #{inspect(v)}"
    end
  end
  defp mime!(row) do
    case Map.get(row, "mime") do
      s when is_binary(s) and byte_size(s) > 2 -> s
      _ ->
        ext = Map.get(row, "Type", Map.get(row, "type"))
        case Map.fetch(@mime, ext) do
          {:ok, m} -> m
          :error -> raise ArgumentError, "unknown MIME/Type: #{inspect(ext)}"
        end
    end
  end
end
