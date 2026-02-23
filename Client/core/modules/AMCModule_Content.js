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

export default class AMCApplicationModule_Content extends Common.AMCApplicationModule {
	
	constructor (page, moduleJSON) 
	{		
		Assert.ObjectValue (moduleJSON);
		
		super (page, moduleJSON.uuid, moduleJSON.type, moduleJSON.name, moduleJSON.caption);		
		this.registerClass ("amcModule_Content");

		// Phase 2: content module can read from v2 frontend attributes
		this.usesV2Frontend = true;
		
		this.headline = Assert.StringValue (moduleJSON.headline);
		this.title = Assert.StringValue (moduleJSON.title);
		this.subtitle = Assert.StringValue (moduleJSON.subtitle);

		this.visible = Assert.BoolValue (moduleJSON.visible);

		this.modules = [];

		if (moduleJSON.modules && Array.isArray(moduleJSON.modules)) {
			for (let childModuleJSON of moduleJSON.modules) {
				let childModule = this.page.application.createModuleInstance(this.page, childModuleJSON);
				if (!childModule)
					throw "Submodule type not found: " + childModuleJSON.type;
				this.modules.push(childModule);
				this.page.application.addModule(childModule);
			}
		}			

		// Backward compatibility: legacy content definitions with "items" are
		// wrapped into leaf modules on the client.
		// The legacy state uses the ContentLeaf shell pattern:
		//   itemJSON = { type, uuid (ContentLeaf UUID), name, caption, visible, items: [innerJSON] }
		// The actual payload (entities, buttons, entries…) lives inside items[0].
		// We merge: identity/visibility from the outer shell, payload from the inner item.
		if (moduleJSON.items && Array.isArray(moduleJSON.items)) {
			for (let itemJSON of moduleJSON.items) {
				let innerJSON = (itemJSON.items && itemJSON.items.length > 0) ? itemJSON.items[0] : itemJSON;

				let wrappedModuleJSON = Object.assign({}, innerJSON);
				wrappedModuleJSON.type    = itemJSON.type || innerJSON.type;
				wrappedModuleJSON.uuid    = itemJSON.uuid;
				wrappedModuleJSON.name    = itemJSON.name  || innerJSON.name  || itemJSON.uuid || (this.name + "_" + (itemJSON.type || ""));
				wrappedModuleJSON.caption = (itemJSON.caption !== undefined) ? itemJSON.caption : (innerJSON.caption || "");
				wrappedModuleJSON.visible = (itemJSON.visible !== undefined) ? itemJSON.visible : true;

				let wrappedModule = this.page.application.createModuleInstance(this.page, wrappedModuleJSON);
				if (!wrappedModule)
					throw "Item type not found: " + (itemJSON.type || innerJSON.type);
				this.modules.push(wrappedModule);
				this.page.application.addModule(wrappedModule);
			}
		}
				
	}

	updateFromJSON(updateJSON) {

		Assert.ObjectValue(updateJSON);

		if(updateJSON.visible !== undefined)
			this.visible = Assert.BoolValue(updateJSON.visible);
	}

	updateFromV2Attributes (attrs) {
		if (attrs.headline !== undefined)
			this.headline = attrs.headline;
		if (attrs.title !== undefined)
			this.title = attrs.title;
		if (attrs.subtitle !== undefined)
			this.subtitle = attrs.subtitle;
		if (attrs.caption !== undefined)
			this.caption = attrs.caption;
		if (attrs.visible !== undefined)
			this.visible = (attrs.visible === "1" || attrs.visible === true || attrs.visible === "true");
		return true;
	}
	
}
