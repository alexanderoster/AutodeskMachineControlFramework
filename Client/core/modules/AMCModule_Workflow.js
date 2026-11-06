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


// Static workflow stub. The catalog is converted to the operation type shape of the operation sequence editor.
export default class AMCApplicationModule_Workflow extends Common.AMCApplicationModule {

	constructor (page, moduleJSON)
	{
		Assert.ObjectValue (moduleJSON);
		super (page, moduleJSON.uuid, moduleJSON.type, moduleJSON.name || moduleJSON.uuid, moduleJSON.caption || "");
		this.registerClass ("amcModule_Workflow");

		this.usesV2Frontend = true;

		this.workflowData = null;
		this.catalogData = null;
		if (moduleJSON.catalog !== undefined)
			this.catalogData = this._normalizeCatalog (moduleJSON.catalog);
	}


	updateFromJSON (updateJSON)
	{
		Assert.ObjectValue (updateJSON);
		if (updateJSON.catalog !== undefined)
			this.catalogData = this._normalizeCatalog (updateJSON.catalog);
		if (updateJSON.visible !== undefined)
			this.visible = Assert.BoolValue (updateJSON.visible);
	}


	updateFromV2Attributes (attrs)
	{
		if (!attrs)
			return true;
		if (attrs.catalog !== undefined)
			this.catalogData = this._normalizeCatalog (attrs.catalog);
		if (attrs.caption !== undefined)
			this.caption = attrs.caption;
		if (attrs.visible !== undefined)
			this.visible = (attrs.visible === "1" || attrs.visible === true || attrs.visible === "true");
		return true;
	}

	_normalizeParameterType (type)
	{
		if (type === "int")
			return "integer";
		if (type === "float")
			return "double";
		if (type === "bool")
			return "bool";
		return "string";
	}

	_normalizeCatalog (rawCatalog)
	{
		if (!Array.isArray (rawCatalog))
			return null;
		return rawCatalog.map (entry => ({
			type: entry.id,
			displayname: entry.label || entry.id,
			category: (entry.category === "template") ? "Templates" : "Function Blocks",
			color: entry.color || "blue",
			contexts: [],
			itemlists: [],
			parameters: (Array.isArray (entry.params) ? entry.params : []).map (p => ({
				name: p.name,
				displayname: p.name,
				type: this._normalizeParameterType (p.type),
				unit: p.unit || undefined,
				default: (p.default !== undefined) ? String (p.default) : "",
				expression: "literal",
				required: false
			}))
		}));
	}

	getDefaultWorkflow ()
	{
		const operation = (uuid, type, values) => ({ uuid: uuid, type: type, enabled: true, comment: "", values: values || {}, items: {} });

		return {
			name: "Example Production",
			displayname: "Example Production",
			description: "",
			context: "",
			bindinggroup: "",
			revision: 0,
			operations: [
				operation ("6f1c0a52-3a7e-4c39-9b51-000000000001", "exposure"),
				operation ("6f1c0a52-3a7e-4c39-9b51-000000000002", "delay", { duration: "3000" }),
				operation ("6f1c0a52-3a7e-4c39-9b51-000000000003", "beschichter_lr"),
				operation ("6f1c0a52-3a7e-4c39-9b51-000000000004", "dosierung_r"),
				operation ("6f1c0a52-3a7e-4c39-9b51-000000000005", "heatup_layer"),
				operation ("6f1c0a52-3a7e-4c39-9b51-000000000006", "exposure")
			]
		};
	}

	getDefaultCatalog ()
	{
		return this._normalizeCatalog ([
			{ id: "exposure",       label: "Exposure",        category: "functionblock", color: "blue" },
			{ id: "delay",          label: "Delay",           category: "functionblock", color: "blue",
			  params: [{ name: "duration", type: "int", unit: "ms", default: 1000 }] },
			{ id: "beschichter_lr", label: "Beschichter L→R", category: "template",      color: "amber" },
			{ id: "beschichter_rl", label: "Beschichter R→L", category: "template",      color: "amber" },
			{ id: "dosierung_r",    label: "Dosierung R",     category: "template",      color: "amber" },
			{ id: "dosierung_l",    label: "Dosierung L",     category: "template",      color: "amber" },
			{ id: "heatup_layer",   label: "Heatup Layer",    category: "template",      color: "amber" },
			{ id: "hold_temp",      label: "Hold Temperature",category: "template",      color: "amber" }
		]);
	}

}
