defmodule Skytrace.Application do
  use Application
  def start(_type, _args) do
    Supervisor.start_link([{Task.Supervisor, name: Skytrace.TaskSupervisor, max_children: 8}],
      strategy: :one_for_one, name: Skytrace.Supervisor)
  end
end
