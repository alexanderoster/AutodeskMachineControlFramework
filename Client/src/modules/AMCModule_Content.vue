<!--

Copyright (C) 2020 Autodesk Inc.

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

		<div width="100%" height="100%" flat v-if="(module.type == 'content')">
          <v-card-title v-if="module.title != ''">{{ module.title }}</v-card-title>
          <v-card-subtitle v-if="(module.subtitle != '')">	{{ module.subtitle }}</v-card-subtitle>
          <div v-if="module.modules && (module.modules.length > 0)" class="text--primary" width="100%">
				<template v-for="childModule in module.modules">
					<Module_ContentLeaf :key="childModule.uuid" v-if="(childModule.type == 'paragraph') || (childModule.type == 'image') || (childModule.type == 'chart') || (childModule.type == 'videostream') || (childModule.type == 'upload') || (childModule.type == 'buildlist') || (childModule.type == 'executionlist') || (childModule.type == 'alertlist') || (childModule.type == 'buttongroup') || (childModule.type == 'parameterlist') || (childModule.type == 'configurationlist') || (childModule.type == 'form')" :module="childModule" :Application="Application" />
					<Module_Tabs :key="childModule.uuid" v-if="(childModule.type == 'tabs')" :module="childModule" :Application="Application" />
					<Module_Grid :key="childModule.uuid" v-if="(childModule.type == 'grid')" :module="childModule" :Application="Application" />
					<Module_Logs :key="childModule.uuid" v-if="(childModule.type == 'logs')" :module="childModule" :Application="Application" />
					<Module_GLScene :key="childModule.uuid" v-if="(childModule.type == 'glscene')" :module="childModule" :Application="Application" />
					<Module_Graphic :key="childModule.uuid" v-if="(childModule.type == 'graphic')" :module="childModule" :Application="Application" />
					<Module_LayerView :key="childModule.uuid" v-if="(childModule.type == 'layerview')" :module="childModule" :Application="Application" />
				</template>
			</div>
	</div>

</template>

<script>

	import Module_ContentLeaf from "../modules/AMCModule_ContentLeaf.vue";
	import Module_Tabs from "../modules/AMCModule_Tabs.vue";
	import Module_Grid from "../modules/AMCModule_Grid.vue";
	import Module_GLScene from "../modules/AMCModule_GLScene.vue";
	import Module_Graphic from "../modules/AMCModule_Graphic.vue";
	import Module_Logs from "../modules/AMCModule_Logs.vue";
	import Module_LayerView from "../modules/AMCModule_LayerView.vue";


	export default {
		props: ["Application", "module"],
	  
		components: {
			Module_ContentLeaf,
			Module_Tabs,
			Module_Grid,
			Module_GLScene,
			Module_Graphic,
			Module_Logs,
			Module_LayerView
		}		
		
	};
	
</script>
