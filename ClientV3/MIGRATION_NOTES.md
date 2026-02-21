# ClientV3 migration notes

## What changed
- Vue 3 + Vuetify 3 with Vite tooling (see `ClientV3/package.json` and `ClientV3/vite.config.mjs`).
- `ClientV3/src/main.js` now uses `createApp` and registers a local `v-resize` directive.
- Core layout updated for Vuetify 3 (`ClientV3/src/App.vue`), tabs now use `v-window` (`ClientV3/src/modules/AMCModule_Tabs.vue`).
- Charts moved to `vue3-apexcharts` (`ClientV3/src/modules/AMCModule_ContentItem_Chart.vue`).

## Follow-up work to validate
- `v-data-table` API changed in Vuetify 3 (headers may require `key` instead of `value`). Verify all data table modules:
  - `ClientV3/src/modules/AMCModule_ContentItem_BuildList.vue`
  - `ClientV3/src/modules/AMCModule_ContentItem_ExecutionList.vue`
  - `ClientV3/src/modules/AMCModule_ContentItem_AlertList.vue`
  - `ClientV3/src/modules/AMCModule_ContentItem_ParameterList.vue`
  - `ClientV3/src/modules/AMCModule_ContentItem_ConfigurationList.vue`
- Icon props in Vuetify 3 differ (`prepend-icon` vs `prepend-inner-icon`). Review login fields in `ClientV3/src/dialogs/Dialog_Login.vue`.
- Layout sizing now reads `appBar` and `footer` heights directly. Confirm `appContentDivStyle` and `appContentCardStyle` sizing in `ClientV3/src/App.vue` under real UI load.
- Verify `v-file-input` behavior in `ClientV3/src/modules/AMCModule_ContentItem_Upload.vue` and update props if needed.

## Build integration
- `Artifacts/build_client_clean.sh` still targets `Client/`. If you want to package `ClientV3/`, duplicate or update the script to copy `ClientV3` sources and run `npm run build` from that directory.
- `ClientV3/package-lock.json` was removed; run `npm install` inside `ClientV3/` to regenerate it.

## Local build commands
- Vite 6 supports Node.js 18+. Upgrade to Node 20.19+ only if you move back to Vite 7.
- `cd ClientV3 && npm install`
- `cd ClientV3 && npm run build`
- `cd ClientV3 && npm run serve`
