# SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-SnapKitty-Opaque-1.0
# Copyright (C) 2026 Ahmad Ali Parr / SnapKitty Research Lab
defmodule Skytrace.MixProject do
  use Mix.Project
  def project, do: [app: :skytrace_beam, version: "1.0.0", elixir: "~> 1.14", deps: []]
  def application, do: [extra_applications: [:logger], mod: {Skytrace.Application, []}]
end
