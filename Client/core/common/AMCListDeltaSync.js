/*++

Copyright (C) 2021 Autodesk Inc.

All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
 * Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.
 * Neither the name of the Autodesk Inc. nor the
names of its contributors may be used to endorse or promote products
derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL AUTODESK INC. BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 */


/*

Incremental synchronisation of a keyed client side list with a server list endpoint.

The server lists (/build, /executions, /alerts) give every row an incremental_id, which is
bumped on every change, and publish the largest one as a "head id" frontend attribute
(buildlistheadid, executionlistheadid, alertlistheadid). The owning module forwards that
attribute to notifyHeadID. The sync then calls the module's fetch function:

	fetchFunction (sinceHeadID) -> Promise of { headID, delta, rows, removedKeys }

with sinceHeadID === null for a full list, or with the head id of the last applied
response for a delta ("?since=<id>"; the server then returns only rows whose
incremental_id is larger, plus the keys of rows that left the list in removedKeys).

- A full response replaces all entries.
- A delta response upserts its rows by key, drops removedKeys and re-sorts.
- The server reads the head id before the rows, so a row that changes during a request
  shows up again in the next delta. Upserts are idempotent, so this is harmless.
- A head id that goes backwards (e.g. a new alert journal after a server restart, or a
  replaced database) cannot be applied as a delta, so it triggers a full resync.
- Only one request is in flight at a time. Notifications that arrive meanwhile are
  coalesced into one follow-up request.
- A failed request leaves the list untouched; the next notification retries from the
  last applied head id.

The entries array is owned by the module (and observed by the UI framework), so it is
only ever modified in place.

*/


function compareValues (valueA, valueB)
{
	let isEmptyA = (valueA === undefined) || (valueA === null) || (valueA === "");
	let isEmptyB = (valueB === undefined) || (valueB === null) || (valueB === "");
	if (isEmptyA || isEmptyB) {
		if (isEmptyA && isEmptyB)
			return 0;
		return isEmptyA ? -1 : 1;
	}

	if ((typeof valueA === "number") && (typeof valueB === "number"))
		return valueA - valueB;

	if ((typeof valueA === "boolean") && (typeof valueB === "boolean"))
		return (valueA === valueB) ? 0 : (valueA ? 1 : -1);

	let stringA = String (valueA);
	let stringB = String (valueB);
	if (stringA < stringB)
		return -1;
	if (stringA > stringB)
		return 1;
	return 0;
}


// Returns a comparison function for Array.sort that orders entries by one field.
// Timestamps are ISO 8601 strings and therefore compare correctly as strings.
// Empty values sort first in ascending and last in descending order.
export function compareEntriesBy (fieldName, descending)
{
	let sign = descending ? -1 : 1;

	return (entryA, entryB) => sign * compareValues (entryA[fieldName], entryB[fieldName]);
}


export default class AMCListDeltaSync {

	constructor (entries, keyField, compareFunction, fetchFunction)
	{
		if (!Array.isArray (entries))
			throw "AMCListDeltaSync: entries must be an array";
		if ((typeof keyField !== "string") || (keyField === ""))
			throw "AMCListDeltaSync: invalid key field";
		if (typeof fetchFunction !== "function")
			throw "AMCListDeltaSync: invalid fetch function";

		this.entries = entries;
		this.keyField = keyField;
		this.compareFunction = (typeof compareFunction === "function") ? compareFunction : null;
		this.fetchFunction = fetchFunction;

		// Head id of the last applied response, null until the first full list has been applied.
		this.syncedHeadID = null;
		// Largest head id announced by the server since the last applied response.
		this.notifiedHeadID = null;
		this.fullResyncRequested = true;
		this.requestPending = false;
		this.requestInFlight = false;
	}


	// Called with the head id frontend attribute whenever it is (re)sent by the server.
	notifyHeadID (headID)
	{
		if ((typeof headID === "number") && !isNaN (headID)) {
			if ((this.syncedHeadID !== null) && (headID < this.syncedHeadID))
				this.fullResyncRequested = true;
			this.notifiedHeadID = headID;
		}

		this.requestPending = true;
		this.processPendingRequest ();
	}


	// Forces the next request to fetch the complete list.
	requestFullResync ()
	{
		this.fullResyncRequested = true;
		this.requestPending = true;
		this.processPendingRequest ();
	}


	isSynced ()
	{
		return (this.syncedHeadID !== null);
	}


	needsRequest ()
	{
		if (this.fullResyncRequested || (this.syncedHeadID === null))
			return true;
		if (this.notifiedHeadID === null)
			return false;
		return (this.notifiedHeadID !== this.syncedHeadID);
	}


	processPendingRequest ()
	{
		if (this.requestInFlight || !this.requestPending)
			return;

		this.requestPending = false;
		if (!this.needsRequest ())
			return;

		let sinceHeadID = null;
		if (!this.fullResyncRequested && (this.syncedHeadID !== null))
			sinceHeadID = this.syncedHeadID;

		this.fullResyncRequested = false;
		this.requestInFlight = true;

		let request;
		try {
			request = Promise.resolve (this.fetchFunction (sinceHeadID));
		} catch (error) {
			request = Promise.reject (error);
		}

		request.then (result => {
			this.requestInFlight = false;
			this.applyResult (result, sinceHeadID);
			this.processPendingRequest ();
		}).catch (() => {
			this.requestInFlight = false;
			// Retry a full list if nothing has ever been applied; otherwise wait for the next notification.
			if (this.syncedHeadID === null)
				this.fullResyncRequested = true;
		});
	}


	applyResult (result, sinceHeadID)
	{
		if (!result || !Array.isArray (result.rows))
			throw "AMCListDeltaSync: invalid fetch result";

		let headID = parseInt (result.headID);
		let isDelta = (result.delta === true);

		if (isDelta && (sinceHeadID === null))
			throw "AMCListDeltaSync: unexpected delta response to a full request";

		// The server lost rows that this delta is based on (e.g. a new journal): fetch everything.
		if (isDelta && (isNaN (headID) || (headID < sinceHeadID))) {
			this.fullResyncRequested = true;
			this.requestPending = true;
			return;
		}

		let newEntries;
		if (isDelta) {
			let removedKeys = new Set (Array.isArray (result.removedKeys) ? result.removedKeys : []);
			let entryMap = new Map ();
			for (let entry of this.entries) {
				let key = entry[this.keyField];
				if (!removedKeys.has (key))
					entryMap.set (key, entry);
			}
			for (let row of result.rows) {
				let key = row[this.keyField];
				if (removedKeys.has (key))
					continue;
				entryMap.set (key, row);
			}
			newEntries = Array.from (entryMap.values ());
		} else {
			let entryMap = new Map ();
			for (let row of result.rows)
				entryMap.set (row[this.keyField], row);
			newEntries = Array.from (entryMap.values ());
		}

		if (this.compareFunction)
			newEntries.sort (this.compareFunction);

		this.entries.splice (0, this.entries.length, ...newEntries);

		if (isNaN (headID)) {
			// Without a head id no delta can be based on this list; fetch fully next time.
			this.syncedHeadID = null;
			this.fullResyncRequested = true;
		} else {
			this.syncedHeadID = headID;
			if ((this.notifiedHeadID === null) || (this.notifiedHeadID < headID))
				this.notifiedHeadID = headID;
			else if (this.notifiedHeadID > headID)
				this.requestPending = true;
		}
	}

}
