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


export default class AMCApplicationModule_Form extends Common.AMCApplicationModule {

	constructor (page, moduleJSON)
	{
		Assert.ObjectValue (moduleJSON);
		super (page, moduleJSON.uuid, moduleJSON.type, moduleJSON.name || moduleJSON.uuid, moduleJSON.caption || "");
		this.registerClass ("amcModule_Form");

		this.usesV2Frontend = true;

		this.entities = Array.isArray(moduleJSON.entities) ? moduleJSON.entities : [];

		for (let entity of this.entities) {
			entity.dataObject = {
				uuid:                 entity.uuid,
				value:                entity.value,
				remotevalue:          entity.value,
				disabled:             entity.disabled,
				readonly:             entity.readonly,
				changeevent:          entity.changeevent,
				validation:           entity.validation,
				validationmessage:    entity.validationmessage,
				minvalue:             entity.minvalue,
				maxvalue:             entity.maxvalue,
				isProgrammaticChange: false
			};

			this.page.application.AppContent.FormEntityMap.set (entity.uuid, entity);
		}
	}


	updateFromJSON (updateJSON)
	{
		Assert.ObjectValue (updateJSON);

		this.setVisible(updateJSON.visible);

		if (updateJSON.entities) {
			Assert.ArrayValue (updateJSON.entities);

			for (let entityJSON of updateJSON.entities) {
				if (!this.page.application.AppContent.FormEntityMap.has (entityJSON.uuid)) {
					// Entity not yet registered — add it now (late-init fallback).
					let newEntity = {
						uuid:              entityJSON.uuid,
						name:              entityJSON.name || entityJSON.uuid,
						type:              entityJSON.type || "edit",
						caption:           entityJSON.caption || "",
						value:             entityJSON.value,
						disabled:          !!entityJSON.disabled,
						readonly:          !!entityJSON.readonly,
						changeevent:       entityJSON.changeevent || "",
						validation:        entityJSON.validation || "",
						validationmessage: entityJSON.validationmessage || "",
						minvalue:          entityJSON.minvalue,
						maxvalue:          entityJSON.maxvalue,
					};
					newEntity.dataObject = {
						uuid:                 newEntity.uuid,
						value:                newEntity.value,
						remotevalue:          newEntity.value,
						disabled:             newEntity.disabled,
						readonly:             newEntity.readonly,
						changeevent:          newEntity.changeevent,
						validation:           newEntity.validation,
						validationmessage:    newEntity.validationmessage,
						minvalue:             newEntity.minvalue,
						maxvalue:             newEntity.maxvalue,
						isProgrammaticChange: false,
					};
					this.entities.push(newEntity);
					this.page.application.AppContent.FormEntityMap.set(newEntity.uuid, newEntity);
				}

				let entity = this.page.application.AppContent.FormEntityMap.get (entityJSON.uuid);
				let dataObject = entity.dataObject;

				Assert.ObjectValue (dataObject);

				if (dataObject.remotevalue != entityJSON.value) {
					dataObject.value = entityJSON.value;
					dataObject.isProgrammaticChange = true;
				} else {
					dataObject.isProgrammaticChange = false;
				}
				dataObject.remotevalue = entityJSON.value;
				dataObject.prefix = entityJSON.prefix;
				dataObject.suffix = entityJSON.suffix;
				dataObject.readonly = entityJSON.readonly;
				dataObject.disabled = entityJSON.disabled;
				dataObject.changeevent = entityJSON.changeevent;
			}
		}
	}


	updateFromV2Attributes (attrs)
	{
		let v2Entry = this.page.application.getV2Entry(this.uuid);
		if (!v2Entry)
			return true;

		if (v2Entry.attributes) {
			if (v2Entry.attributes.visible !== undefined)
				this.setVisible(v2Entry.attributes.visible === true || v2Entry.attributes.visible === "1" || v2Entry.attributes.visible === "true");
			if (v2Entry.attributes.caption !== undefined)
				this.caption = v2Entry.attributes.caption;
		}

		if (v2Entry.submodules) {
			let app = this.page.application;

			for (let sub of v2Entry.submodules) {
				if (!app.AppContent.FormEntityMap.has(sub.uuid))
					continue;

				let entity = app.AppContent.FormEntityMap.get(sub.uuid);
				let dataObject = entity.dataObject;
				let a = sub.attributes || {};

				if (a.value !== undefined) {
					if (dataObject.remotevalue !== a.value) {
						dataObject.value = a.value;
						dataObject.isProgrammaticChange = true;
					} else {
						dataObject.isProgrammaticChange = false;
					}
					dataObject.remotevalue = a.value;
				}

				if (a.caption !== undefined)
					entity.caption = a.caption;
				if (a.disabled !== undefined)
					dataObject.disabled = (a.disabled === true || a.disabled === "1" || a.disabled === "true");
				if (a.readonly !== undefined)
					dataObject.readonly = (a.readonly === true || a.readonly === "1" || a.readonly === "true");
				if (a.prefix !== undefined)
					dataObject.prefix = a.prefix;
				if (a.suffix !== undefined)
					dataObject.suffix = a.suffix;
				if (a.changeevent !== undefined)
					dataObject.changeevent = a.changeevent;
			}
		}

		return true;
	}


	setVisible (flag) {
		this.visible = !!flag;
	}

}
