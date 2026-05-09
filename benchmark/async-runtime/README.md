# Async Runtime Benchmark Boundary

The async runtime benchmark framework lives in:

```text
styio-benchmark/async-runtime/
```

Do not add async performance workloads, runners, generated peer-runtime sources,
or stored reports under the Styio repository. Styio only provides the selected
checkout, build directory, and task scheduler probe that the external framework
uses through `--styio-root`.
