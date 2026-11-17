# SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-SnapKitty-Opaque-1.0
# Copyright (C) 2026 Ahmad Ali Parr / SnapKitty Research Lab
defmodule Skytrace.IngestTest do
  use ExUnit.Case
  test "ordered source snapshot and cryptographic hash" do
    p = Path.expand("../../fixtures/archives/synthetic-batch.json", __DIR__)
    results = Skytrace.Ingest.fanout([{"pursue", p}])
    assert [{:ok, %{source: "pursue", records: rows, sha256: hash}}] = results
    assert length(rows) == 3
    assert byte_size(hash) == 64
  end
  test "worker crashes are isolated" do
    results = Skytrace.Ingest.fanout([{"pursue", "/missing/file"}])
    assert [{:error, _}] = results
  end
end

defmodule Skytrace.AdaptersTest do
  use ExUnit.Case
  test "PURSUE typed source row is mapped without inventing missing telemetry" do
    row = %{"Record ID" => "DOW-UAP-TEST", "Agency" => "DOW", "Release Date" => "2026-10-23", "Type" => "pdf"}
    out = Skytrace.Adapters.adapt!("pursue", row)
    assert out["mime"] == "application/pdf" and out["has_range_telemetry"] == nil
  end
  test "AARO resolved cases remain indexed with resolved marker" do
    row = %{"case_id" => "PR-TEST", "release_date" => "2026-10-01", "mime" => "video/mp4", "resolution_status" => "resolved"}
    assert Skytrace.Adapters.adapt!("aaro", row)["resolved"] == true
  end
  test "NARA missing canonical ID fails closed" do
    assert_raise ArgumentError, fn -> Skytrace.Adapters.adapt!("nara", %{"publicationDate" => "2026-01-01"}) end
  end
end
