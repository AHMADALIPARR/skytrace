defmodule Skytrace.MixProject do
  use Mix.Project
  def project, do: [app: :skytrace_beam, version: "1.0.0", elixir: "~> 1.14", deps: []]
  def application, do: [extra_applications: [:logger], mod: {Skytrace.Application, []}]
end
