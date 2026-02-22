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

import AMCApplicationItem_Content_Chart from "./AMCModule_ContentItem_Chart.js"
import AMCApplicationItem_Content_Image from "./AMCModule_ContentItem_Image.js"
import AMCApplicationItem_Content_Paragraph from "./AMCModule_ContentItem_Paragraph.js"
import AMCApplicationItem_Content_VideoStream from "./AMCModule_ContentItem_VideoStream.js"

export default class AMCApplicationModule_ContentLeaf extends Common.AMCApplicationModule {

	constructor (page, moduleJSON)
	{
		Assert.ObjectValue (moduleJSON);

		super (page, moduleJSON.uuid, moduleJSON.type, moduleJSON.name, moduleJSON.caption);
		this.registerClass ("amcModule_ContentLeaf");
		this.usesV2Frontend = true;

		this.visible = true;
		if (moduleJSON.visible !== undefined)
			this.visible = (moduleJSON.visible === "1" || moduleJSON.visible === true || moduleJSON.visible === "true");

		this.items = [];

		let itemJSON = Object.assign({}, moduleJSON);
		itemJSON.type = moduleJSON.type;
		itemJSON.uuid = moduleJSON.uuid;

		// v2 uses "resource" for image modules; legacy image item expects "imageresource".
		if ((itemJSON.type === "image") && (itemJSON.imageresource === undefined) && (itemJSON.resource !== undefined))
			itemJSON.imageresource = itemJSON.resource;

		// Keep a compatibility alias if stream resources are exposed via "resource".
		if ((itemJSON.type === "videostream") && (itemJSON.streamresource === undefined) && (itemJSON.resource !== undefined))
			itemJSON.streamresource = itemJSON.resource;

		let item = null;
		if (itemJSON.type === "paragraph")
			item = new AMCApplicationItem_Content_Paragraph (this, itemJSON);
		if (itemJSON.type === "image")
			item = new AMCApplicationItem_Content_Image (this, itemJSON);
		if (itemJSON.type === "chart")
			item = new AMCApplicationItem_Content_Chart (this, itemJSON);
		if (itemJSON.type === "videostream")
			item = new AMCApplicationItem_Content_VideoStream (this, itemJSON);

		if (item) {
			this.items.push(item);
			this.page.addItem(item);
		} else {
			throw "Leaf module type not found: " + itemJSON.type;
		}
	}

	updateFromJSON(updateJSON) {
		Assert.ObjectValue(updateJSON);

		if (updateJSON.caption !== undefined)
			this.caption = updateJSON.caption;
		if (updateJSON.visible !== undefined)
			this.visible = (updateJSON.visible === "1" || updateJSON.visible === true || updateJSON.visible === "true");

		let item = this.items[0];
		if (item && typeof item.updateFromJSON === "function")
			item.updateFromJSON(updateJSON);
	}

	updateFromV2Attributes (attrs) {
		if (!attrs)
			return true;

		if (attrs.caption !== undefined)
			this.caption = attrs.caption;
		if (attrs.visible !== undefined)
			this.visible = (attrs.visible === "1" || attrs.visible === true || attrs.visible === "true");

		return true;
	}
}
