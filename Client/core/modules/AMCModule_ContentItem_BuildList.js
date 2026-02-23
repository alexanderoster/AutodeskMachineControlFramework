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


import * as Assert from "../common/AMCAsserts.js";
import * as Common from "../common/AMCCommon.js"


export default class AMCApplicationItem_Content_BuildList extends Common.AMCApplicationItem {
	
	constructor (moduleInstance, itemJSON) 
	{
		Assert.ObjectValue (itemJSON);		
		super (moduleInstance, itemJSON.uuid, itemJSON.type);		
		this.registerClass ("amcItem_BuildList");

		this.entries = [];
				
		this.headers = [];
		for (let header of itemJSON.headers) {
			let checkedHeader = {
				"text": header.text,
				"value": header.value,						
				"sortable": header.sortable,
				"width": header.width
			}
			
			this.headers.push(checkedHeader);
		}

		this.entrybuttons = [];
		if (itemJSON.entrybuttons) {
			for (let entrybutton of itemJSON.entrybuttons) {				
				
				let checkedEntryButton = {
					"uuid": entrybutton.uuid,
					"caption": entrybutton.caption,
					"color": entrybutton.color,
					"cursor": entrybutton.cursor,
					"selectevent": entrybutton.selectevent
				}
				
				this.entrybuttons.push(checkedEntryButton);
			}
		}

		
		this.loadingtext = "";
		this.selectevent = "";
		this.selectionvalueuuid = Common.nullUUID ();
		this.buttonvalueuuid = Common.nullUUID ();
		this.thumbnailaspectratio = 1.8;
		this.thumbnailheight = "150pt";
		this.thumbnailwidth = "";
		this.entriesperpage = 25;

		// Phase 2: v2 change-detection via buildlistheadid
		this.usesV2Frontend = true;
		this.lastKnownHeadID = 0;
		this.buildFetchInFlight = false;
		
		/*  this.entrybuttons = [
		{
			uuid: "123",
			caption: "Details",
			color: "primary",
			cursor: "cursor-pointer",
			selectionvalueuuid: Common.nullUUID (),
			selectevent: "12324353",
							
		},
		{
			uuid: "234",
			caption: "History",
			color: "primary",
			cursor: "cursor-pointer",
			selectionvalueuuid: Common.nullUUID (),
			selectevent: "12324353",						
		}
		]; */
		
		this.updateFromJSON (itemJSON);
		
		this.setRefreshFlag ();
								
	}
	
	
	updateFromJSON (updateJSON)
	{
		Assert.ObjectValue (updateJSON);
		
		if (updateJSON.loadingtext)
			this.loadingtext = Assert.StringValue (updateJSON.loadingtext);
		if (updateJSON.selectevent)
			this.selectevent = Assert.IdentifierString (updateJSON.selectevent);		
		if (updateJSON.selectionvalueuuid)
			this.selectionvalueuuid = Assert.IdentifierString (updateJSON.selectionvalueuuid);
		if (updateJSON.buttonvalueuuid)
			this.buttonvalueuuid = Assert.IdentifierString (updateJSON.buttonvalueuuid);
		if (updateJSON.entriesperpage)
			this.entriesperpage = Assert.IntegerValue (updateJSON.entriesperpage);

		if (updateJSON.entries) {
			let oldEntryCount = this.entries.length;
			for (let index = 0; index < oldEntryCount; index++) {
				this.entries.pop();
			}

			for (let entry of updateJSON.entries) {
				this.entries.push(entry);
			}
		}
	}

	updateFromV2Attributes (attrs)
	{
		if (attrs.loadingtext !== undefined)
			this.loadingtext = attrs.loadingtext;
		if (attrs.selectevent !== undefined)
			this.selectevent = attrs.selectevent;
		if (attrs.entriesperpage !== undefined)
			this.entriesperpage = parseInt(attrs.entriesperpage) || this.entriesperpage;

		if (attrs.buildlistheadid === undefined)
			return true;

		let headID = parseInt(attrs.buildlistheadid);
		if (headID <= this.lastKnownHeadID)
			return true;

		this.lastKnownHeadID = headID;

		if (this.buildFetchInFlight)
			return true;

		this.buildFetchInFlight = true;

		let app = this.moduleInstance.page.application;

		app.axiosGetRequest("/build?status=validated")
		.then(resultJSON => {
			this.buildFetchInFlight = false;

			if (resultJSON.data && resultJSON.data.buildjobs) {
				let newEntries = [];

				for (let job of resultJSON.data.buildjobs) {
					let entry = {
						buildUUID: job.uuid || "",
						buildName: job.name || "",
						buildLayers: job.layercount || 0,
						buildTimestamp: job.timestamp || "",
						buildUser: job.user || "",
						buildExecutionCount: job.executioncount || 0,
						buildThumbnail: job.thumbnail || "00000000-0000-0000-0000-000000000000"
					};
					newEntries.push(entry);
				}

				let oldCount = this.entries.length;
				for (let i = 0; i < oldCount; i++) {
					this.entries.pop();
				}
				for (let entry of newEntries) {
					this.entries.push(entry);
				}
			}
		})
		.catch(() => {
			this.buildFetchInFlight = false;
		});

		return true;
	}

		
}
