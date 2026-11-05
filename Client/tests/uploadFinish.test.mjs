// Run with: cd Client/tests && npm test   (or from the repository root: node --test 'Client/tests/*.test.mjs')
//
// /upload/finish makes the server hash the whole stream (twice: SHA-256 and blockwise SHA-256), so its
// duration grows with the file size. On slow hardware a large file takes longer than the default 10 s
// request timeout: the client aborted the request, swallowed the error and the upload widget stayed at
// 100 % forever, although the server had stored and validated the file. The finish request therefore
// runs without a timeout (like the chunk POSTs), and any failed upload request is reported to the
// upload state so the frontend can show it.

import { test } from "node:test";
import assert from "node:assert/strict";

import { importCore } from "./support/coreLoader.mjs";

const AMCApplication = await importCore ("common/AMCApplication.js");
const AMCUploadState = await importCore ("common/AMCImplementation_UploadState.js");

const ITEM_UUID = "00000000-0000-4000-8000-000000000001";
const STREAM_UUID = "00000000-0000-4000-8000-000000000002";
const CONTEXT_UUID = "00000000-0000-4000-8000-000000000003";

// Node has Blob and File but no FileReader; AMCUpload only uses readAsArrayBuffer + onload.
globalThis.FileReader = class {
	readAsArrayBuffer (blob) {
		blob.arrayBuffer ().then ((buffer) => { this.result = buffer; this.onload (); });
	}
};

function flushPromises () {
	return new Promise ((resolve) => setImmediate (resolve));
}

async function settle () {
	for (let i = 0; i < 20; i++)
		await flushPromises ();
}

// finish: how the /upload/finish request ends ("resolve" or "reject").
function createUpload (finish) {
	const app = new AMCApplication ("http://127.0.0.1:1/api", null);
	app.API.authToken = "1".repeat (64);

	const requests = { posts: [], events: [] };
	app.axiosPostRequest = (url, data, config) => {
		requests.posts.push ({ url, data, config });
		if (url === "/upload/")
			return Promise.resolve ({ data: { streamuuid: STREAM_UUID } });
		if (url.endsWith ("/finish")) {
			if (finish === "reject")
				return Promise.reject (new Error ("timeout of 10000ms exceeded"));
			return Promise.resolve ({ data: { contextuuid: CONTEXT_UUID } });
		}
		return Promise.reject (new Error ("unexpected POST " + url));
	};
	app.axiosPostFormData = () => Promise.resolve ({ data: {} });
	app.triggerUIEvent = (eventName, sender, values) => requests.events.push ({ eventName, sender, values });

	const state = new AMCUploadState (ITEM_UUID);
	state.setChosenFile (new File ([new Uint8Array (1024)], "part.mp4", { type: "video/mp4" }));
	state.generateUploadID ();

	const callbacks = { complete: 0, error: 0 };
	state._onComplete = () => { callbacks.complete++; };
	state._onError = () => { callbacks.error++; };

	return { app, state, requests, callbacks };
}

for (const [label, start] of [
	["image", (app, state) => app.performImageUpload (state, "onsuccess", "onfailure")],
	["build", (app, state) => app.performJobUpload (state, "onsuccess", "onfailure")],
]) {
	test (`${label} upload: the finish request has no timeout`, async () => {
		const { app, state, requests, callbacks } = createUpload ("resolve");
		start (app, state);
		await settle ();

		const finish = requests.posts.find ((post) => post.url.endsWith ("/finish"));
		assert.ok (finish, "finish request sent");
		assert.equal (finish.config?.timeout, 0);
		assert.equal (callbacks.complete, 1);
		assert.deepEqual (requests.events.map ((event) => event.eventName), ["onsuccess"]);
	});

	test (`${label} upload: a failed finish request is reported to the upload state`, async () => {
		const { app, state, requests, callbacks } = createUpload ("reject");
		start (app, state);
		await settle ();

		assert.equal (callbacks.error, 1);
		assert.equal (callbacks.complete, 0);
		assert.deepEqual (requests.events, []);
		assert.equal (state.getUploadID (), 0, "upload no longer active");
	});
}
