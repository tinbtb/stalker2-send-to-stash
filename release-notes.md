# v0.2.0

- Add the reviewed Steam v2.1.0 native layout while retaining Steam v2.0.6 and the supplied alternative layout.
- Keep one shared gameplay implementation and full startup fingerprint/reference validation; no hash bypass or per-frame scanning.
- Log `ACTIVE: SendToStash loaded successfully` only after both inventory hooks are enabled.
- Select the container pool explicitly for each validated layout.

All three inputs passed the local offline resolver/regression matrix. Built DLLs passed PE/import/export and unsupported-host checks. Hold Fire additionally passed geometry, native lookup and frame-capture tests. CI does not run the game or contain proprietary game inputs. Candidate v2.1.0 startup logs confirmed activation; complete gameplay/save regression of the new release remains pending.

Choose ASI-only when a loader is already installed. Keep one ASI copy and preserve your existing loader and settings. Optional-loader packages retain the pinned Ultimate ASI Loader 9.7.4; they do not require replacing an already working loader. Use a copied save for initial gameplay checks.
