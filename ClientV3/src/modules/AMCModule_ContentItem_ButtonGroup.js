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


export default class AMCApplicationItem_Content_ButtonGroup extends Common.AMCApplicationItem {
	
	constructor (moduleInstance, itemJSON) 
	{		
		Assert.ObjectValue (itemJSON);		
		super (moduleInstance, itemJSON.uuid, itemJSON.type);		
		this.registerClass ("amcItem_ButtonGroup");
		
		this.usesV2Frontend = true;
		
		Assert.ArrayValue (itemJSON.buttons);		
		this.buttons = itemJSON.buttons;
		
		this.buttoncssstyle = ""; 
		this.cssstyle = "";
		
		if (itemJSON.buttondistribution === "rightaligned") {
			this.cssstyle = this.cssstyle + "text-align: right;"; 
		}

		if (itemJSON.buttondistribution === "leftaligned") {
			this.cssstyle = this.cssstyle + "text-align: left;"; 
		}

		if (itemJSON.buttondistribution === "centered") {
			this.cssstyle = this.cssstyle + "text-align: centered;"; 
		}

		if (itemJSON.buttondistribution === "equal") {
			
			let buttoncount = this.buttons.length;
			if (buttoncount > 0) {
				this.buttoncssstyle = this.buttoncssstyle + "min-width: calc(" + (100.0 / buttoncount) + "% - 10px);"; 
			}
			
			this.cssstyle = this.cssstyle + "text-align: center;";	
		}
		
		this.setRefreshFlag ();
		
				
	}
	
	
	updateFromJSON (updateJSON)
	{
		Assert.ObjectValue (updateJSON);
		
		if (updateJSON.buttons) {
			this.buttons = updateJSON.buttons;
		}
		
	}
	
	
	updateFromV2Attributes ()
	{
		let v2Entry = this.getApplication().getV2Entry(this.uuid);
		if (v2Entry && v2Entry.submodules) {
			this.buttons = v2Entry.submodules.map(sub => {
				let a = sub.attributes || {};
				let formValues = [];
				if (a.eventformvalues && a.eventformvalues.trim() !== "") {
					formValues = a.eventformvalues.split(" ");
				}
				return {
					uuid: sub.uuid,
					name: sub.uuid,
					caption: a.caption || "",
					disabled: (a.disabled === true || a.disabled === "1" || a.disabled === "true"),
					event: a.event || "",
					targetpage: a.targetpage || "",
					icon: a.icon || "",
					eventformvalues: formValues
				};
			});
		}
		return true;
	}
		
}
