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

<div v-if="(moduleitem.type=='configurationlist')">  	
<v-container>
    <!-- Data table for configuration jobs -->
    <v-data-table
      :headers="moduleitem.headers"
      :items="moduleitem.entries"
      :items-per-page="moduleitem.entriesperpage"
      class="elevation-1"
	  search
	  loadingText="moduleitem.loadingtext"
      :hide-default-footer="false"
    >
		<template v-slot:[`item.configurationActive`]="{ item }">
	<div class="d-flex justify-center">
		<v-checkbox
		v-model="item.configurationActive"
		readonly
		hide-details
		density="compact"
		@click.stop
		></v-checkbox>
	</div>
	</template>
	<template v-slot:[`item.configurationVersion`]="{ item }">
		<div class="d-flex justify-center">
			{{ item.configurationVersion }}
		</div>
	</template>
	<template v-slot:[`item.configurationTimestamp`]="{ item }">
	<div class="d-flex justify-center">
		{{ item.configurationTimestamp }}
	</div>
	</template>	
	<template v-slot:[`item.userName`]="{ item }">
	<div class="d-flex justify-center">
		{{ item.userName }}
	</div>
	</template>

	<template v-slot:[`item.configurationActions`]="{ item }">
	<div class="d-flex justify-center">
		<template v-for="button in getActionButtons(item)">
			<span :key="button.name" @click.stop="onActionButtonClick(button, item)">
			<v-btn color="primary" class="ml-1 mr-1" :style="moduleitem.buttoncssstyle" :disabled="button.disabled">
				<v-icon v-if="button.icon">{{ button.icon }}</v-icon>
				{{ button.caption }}
			</v-btn>
			</span>
		</template>
	</div>
	</template>
     
    </v-data-table>
  </v-container>
</div>

</template>

<script>

	export default {
	  props: ["Application", "moduleitem"],
	  
	  methods: {	

		getActionButtons() {
		return [
		  {
			name: "load",
			caption: "Load",
			icon: "mdi-pencil",
			disabled: false,
			event: "loadconfiguration"
		  },
		  {
			name: "setactive",
			caption: "Set Active",
			icon: "mdi-check",
			disabled: false,
			event: "setactiveconfiguration"
		  },
		  {
			name: "delete",
			caption: "Delete",
			icon: "mdi-delete",
			disabled: false,
			event: "deleteconfiguration"
		  }
		];
	  },

	  onActionButtonClick(button, item) {
		console.log(`Button clicked: ${button.name}`, item);

		const formvalues = {};

		
		if (this.moduleitem.selectionvalueuuid && item.configurationUUID) {
			formvalues[this.moduleitem.selectionvalueuuid] = item.configurationUUID;
		}

		
		if (button.event) {
			this.Application.triggerUIEvent(button.event, this.moduleitem.uuid, formvalues);
		}
		},  

	  }
	};
	
</script>