/*++

Copyright (C) 2026 Autodesk Inc.

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


function parseBool (value, defaultValue)
{
	if (value === undefined || value === null)
		return defaultValue;
	return (value === true || value === "1" || value === "true");
}

// Display options of the user management widget. They can only hide features; what a
// user may actually do is decided by the server (/api/users/meta).
export default class AMCApplicationModule_UserManagement extends Common.AMCApplicationModule {

	constructor (page, moduleJSON)
	{
		Assert.ObjectValue (moduleJSON);
		super (page, moduleJSON.uuid, moduleJSON.type, moduleJSON.name || moduleJSON.uuid, moduleJSON.caption || "");
		this.registerClass ("amcModule_UserManagement");

		this.usesV2Frontend = true;

		this.title = "";
		this.mode = "admin";
		this.columns = "login,description,role,language,online";
		this.roles = "";
		this.entriesperpage = 25;

		this.allowcreate = true;
		this.allowedit = true;
		this.allowroleedit = true;
		this.allowpasswordreset = true;
		this.allowdelete = true;
		this.allowsessions = true;
		this.allowownpassword = true;
		this.showrolepermissions = true;
		this.confirmdelete = true;

		this.updateFromJSON (moduleJSON);
	}


	applyAttributes (attrs)
	{
		if (attrs.title !== undefined)
			this.title = String (attrs.title);
		if (attrs.mode !== undefined)
			this.mode = (String (attrs.mode).toLowerCase () === "self") ? "self" : "admin";
		if (attrs.columns !== undefined)
			this.columns = String (attrs.columns);
		if (attrs.roles !== undefined)
			this.roles = String (attrs.roles);
		if (attrs.entriesperpage !== undefined) {
			let nEntries = parseInt (attrs.entriesperpage, 10);
			if (!isNaN (nEntries) && nEntries > 0)
				this.entriesperpage = nEntries;
		}

		this.allowcreate = parseBool (attrs.allowcreate, this.allowcreate);
		this.allowedit = parseBool (attrs.allowedit, this.allowedit);
		this.allowroleedit = parseBool (attrs.allowroleedit, this.allowroleedit);
		this.allowpasswordreset = parseBool (attrs.allowpasswordreset, this.allowpasswordreset);
		this.allowdelete = parseBool (attrs.allowdelete, this.allowdelete);
		this.allowsessions = parseBool (attrs.allowsessions, this.allowsessions);
		this.allowownpassword = parseBool (attrs.allowownpassword, this.allowownpassword);
		this.showrolepermissions = parseBool (attrs.showrolepermissions, this.showrolepermissions);
		this.confirmdelete = parseBool (attrs.confirmdelete, this.confirmdelete);

		if (attrs.caption !== undefined)
			this.caption = String (attrs.caption);
	}


	updateFromJSON (updateJSON)
	{
		Assert.ObjectValue (updateJSON);
		this.applyAttributes (updateJSON);
	}


	updateFromV2Attributes (attrs)
	{
		if (!attrs)
			return true;

		this.applyAttributes (attrs);

		if (attrs.visible !== undefined)
			this.visible = parseBool (attrs.visible, true);

		return true;
	}

}
