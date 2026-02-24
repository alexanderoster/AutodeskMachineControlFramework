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

<div v-if="module.visible !== false" class="form-root">
	<template v-for="entity in module.entities">

		<!-- Text input -->
		<div :key="entity.uuid + '_edit'" v-if="entity.type === 'edit'" class="form-field">
			<v-text-field
				outlined
				dense
				v-model="entity.dataObject.value"
				:label="entity.caption"
				:disabled="entity.dataObject.disabled"
				:readonly="entity.dataObject.readonly"
				:prefix="entity.prefix"
				:suffix="entity.suffix"
				:rules="checkRules(entity)"
				hide-details="auto"
				class="form-input"
				@blur="uiEditBoxChange(entity)"
				@keyup.enter="uiEditBoxChange(entity)"
			/>
		</div>

		<!-- Toggle switch -->
		<div :key="entity.uuid + '_switch'" v-if="entity.type === 'switch'" class="form-field form-field--switch">
			<span class="form-switch-label">{{ entity.caption }}</span>
			<v-switch
				dense
				hide-details
				inset
				v-model="entity.dataObject.value"
				class="form-switch"
				@change="uiToggleSwitch(entity)"
			/>
		</div>

		<!-- Memo / textarea -->
		<div :key="entity.uuid + '_memo'" v-if="entity.type === 'memo'" class="form-field">
			<v-textarea
				outlined
				dense
				:label="entity.caption"
				v-model="entity.dataObject.value"
				:disabled="entity.dataObject.disabled"
				:readonly="entity.dataObject.readonly"
				hide-details="auto"
				class="form-input"
			/>
		</div>

		<!-- Select / combobox -->
		<div :key="entity.uuid + '_combo'" v-if="entity.type === 'combobox'" class="form-field">
			<v-select
				outlined
				dense
				v-model="entity.dataObject.value"
				:label="entity.caption"
				:disabled="entity.dataObject.disabled"
				:readonly="entity.dataObject.readonly"
				:items="entity.items"
				hide-details="auto"
				class="form-input"
				@input="uiComboboxChange(entity)"
			/>
		</div>

	</template>
</div>

</template>

<script>
export default {
	props: ['Application', 'module'],

	methods: {
		uiToggleSwitch(switchentity) {
			if (!switchentity.dataObject) return;
			if (!switchentity.dataObject.isProgrammaticChange) {
				if (switchentity.changeevent && switchentity.changeevent !== '') {
					const formvalues = this.Application.assembleFormValues([switchentity.uuid]);
					this.Application.triggerUIEvent(switchentity.changeevent, switchentity.uuid, formvalues, () => {
						switchentity.dataObject.isProgrammaticChange = true;
						switchentity.dataObject.value = switchentity.dataObject.remotevalue;
					});
				}
			} else {
				switchentity.dataObject.isProgrammaticChange = false;
			}
		},

		uiComboboxChange(comboboxEntity) {
			if (!comboboxEntity.dataObject) return;
			if (!comboboxEntity.dataObject.isProgrammaticChange) {
				if (comboboxEntity.changeevent && comboboxEntity.changeevent !== '') {
					const formvalues = this.Application.assembleFormValues([comboboxEntity.uuid]);
					this.Application.triggerUIEvent(comboboxEntity.changeevent, comboboxEntity.uuid, formvalues, () => {
						comboboxEntity.dataObject.isProgrammaticChange = true;
						comboboxEntity.dataObject.value = comboboxEntity.dataObject.remotevalue;
					});
				}
			} else {
				comboboxEntity.dataObject.isProgrammaticChange = false;
			}
		},

		uiEditBoxChange(editentity) {
			if (!editentity || !editentity.dataObject) return;
			if (!editentity.changeevent || editentity.changeevent === '') return;
			if (editentity.dataObject.readonly || editentity.dataObject.disabled) return;
			const formvalues = this.Application.assembleFormValues([editentity.uuid]);
			this.Application.triggerUIEvent(editentity.changeevent, editentity.uuid, formvalues);
		},

		checkRules(editentity) {
			const rules = [];
			if (!editentity) return rules;
			if (editentity.validation === 'double') {
				rules.push(v => !!v || editentity.validationmessage);
				rules.push(v => !isNaN(v) || editentity.validationmessage);
				rules.push(v => (v >= editentity.minvalue && v <= editentity.maxvalue) || editentity.validationmessage);
			}
			if (editentity.validation === 'string') {
				rules.push(v => (v !== null && v !== undefined && v !== '') || editentity.validationmessage);
				if (editentity.minlength != null || editentity.maxlength != null) {
					rules.push(v => {
						const len = (v || '').length;
						const minOk = editentity.minlength == null || len >= editentity.minlength;
						const maxOk = editentity.maxlength == null || len <= editentity.maxlength;
						return (minOk && maxOk) || editentity.validationmessage;
					});
				}
			}
			return rules;
		},
	},
};
</script>

<style scoped>
.form-root {
	display: flex;
	flex-direction: column;
	gap: 4px;
	width: 100%;
	min-height: 0;
}

.form-field {
	width: 100%;
}

.form-field--switch {
	display: flex;
	flex-direction: row;
	align-items: center;
	justify-content: space-between;
	min-height: 40px;
	padding: 0 2px;
}

.form-switch-label {
	font-size: 0.875rem;
	color: rgba(0, 0, 0, 0.87);
}

.form-switch {
	flex: 0 0 auto;
	margin: 0 !important;
}

.form-input >>> .v-input__control > .v-input__slot {
	min-height: 36px !important;
}

.form-input >>> .v-text-field__slot label,
.form-input >>> .v-select__slot label {
	font-size: 0.875rem;
}

.form-input >>> .v-text-field__slot input,
.form-input >>> .v-select__slot .v-select__selection {
	font-size: 0.875rem;
}

.form-input >>> fieldset {
	border-radius: 6px;
}
</style>
