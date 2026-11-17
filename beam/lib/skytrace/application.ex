# SPDX-License-Identifier: AGPL-3.0-only OR LicenseRef-SnapKitty-Opaque-1.0
# Copyright (C) 2026 Ahmad Ali Parr / SnapKitty Research Lab
defmodule Skytrace.Application do
  use Application
  def start(_type, _args) do
    Supervisor.start_link([{Task.Supervisor, name: Skytrace.TaskSupervisor, max_children: 8}],
      strategy: :one_for_one, name: Skytrace.Supervisor)
  end
end
