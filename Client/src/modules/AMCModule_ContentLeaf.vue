<!--

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

!-->

<template>

	<div width="100%" height="100%" flat v-if="module.visible && ((module.type == 'paragraph') || (module.type == 'image') || (module.type == 'chart') || (module.type == 'videostream'))">
		<template v-for="moduleitem in module.items">

			<ContentItem_Paragraph :key="moduleitem.uuid" v-if="(moduleitem.type=='paragraph')" :moduleitem="moduleitem" :Application="Application" />

			<ContentItem_Image :key="moduleitem.uuid" v-if="(moduleitem.type=='image')" :moduleitem="moduleitem" :Application="Application" />

			<ContentItem_Chart :key="moduleitem.uuid" v-if="(moduleitem.type=='chart')" :moduleitem="moduleitem" :Application="Application" />

			<ContentItem_VideoStream :key="moduleitem.uuid" v-if="(moduleitem.type=='videostream')" :moduleitem="moduleitem" :Application="Application" />

		</template>
	</div>

</template>

<script>

	import ContentItem_Paragraph from '../modules/AMCModule_ContentItem_Paragraph.vue';
	import ContentItem_Image from '../modules/AMCModule_ContentItem_Image.vue';
	import ContentItem_Chart from '../modules/AMCModule_ContentItem_Chart.vue';
	import ContentItem_VideoStream from '../modules/AMCModule_ContentItem_VideoStream.vue';


	export default {
		props: ["Application", "module"],

		components: {
			ContentItem_Paragraph,
			ContentItem_Image,
			ContentItem_Chart,
			ContentItem_VideoStream
		}

	};

</script>
