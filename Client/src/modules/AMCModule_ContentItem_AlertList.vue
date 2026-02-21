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

<div v-if="moduleitem.type === 'alertlist'" class="alist-root">
	<v-data-table
		:headers="computedHeaders"
		:items="moduleitem.entries"
		:items-per-page="moduleitem.entriesperpage || -1"
		class="alist-table"
		disable-pagination
		hide-default-footer
		width="100%"
		@click:row="uiModuleAlertListClick"
	>
		<template v-slot:[`item.severity`]="{ item }">
			<span v-if="item.severity" class="alist-badge" :class="severityClass(item.severity)">
				{{ item.severity }}
			</span>
		</template>

		<template v-slot:no-data>
			<div class="alist-empty">No alerts</div>
		</template>
	</v-data-table>
</div>

</template>

<script>
const SEVERITY_CLASSES = {
	error:   'alist-badge--red',
	critical:'alist-badge--red',
	warning: 'alist-badge--yellow',
	warn:    'alist-badge--yellow',
	info:    'alist-badge--blue',
	debug:   'alist-badge--slate',
	ok:      'alist-badge--green',
};

export default {
	props: ['Application', 'moduleitem'],

	computed: {
		computedHeaders() {
			if (this.moduleitem.headers && this.moduleitem.headers.length > 0)
				return this.moduleitem.headers;
			return [];
		},
	},

	methods: {
		severityClass(severity) {
			return SEVERITY_CLASSES[(severity || '').toLowerCase()] || 'alist-badge--slate';
		},

		uiModuleAlertListClick(item) {
			if (item && this.moduleitem.selectevent && this.moduleitem.selectionvalueuuid) {
				const eventValues = {};
				eventValues[this.moduleitem.selectionvalueuuid] = item.buildUUID;
				this.Application.triggerUIEvent(this.moduleitem.selectevent, this.moduleitem.uuid, eventValues);
			}
		},
	},
};
</script>

<style scoped>
.alist-root {
	width: 100%;
}

.alist-table {
	border: 1px solid rgba(0, 0, 0, 0.1);
	border-radius: 6px;
	overflow: hidden;
	cursor: pointer;
}

.alist-table >>> thead th {
	font-size: 0.75rem !important;
	font-weight: 600 !important;
	color: rgba(0, 0, 0, 0.55) !important;
	text-transform: none !important;
	letter-spacing: 0 !important;
	border-bottom: 1px solid rgba(0, 0, 0, 0.1) !important;
	padding: 0 16px !important;
	height: 36px !important;
	background: #fafafa !important;
}

.alist-table >>> tbody td {
	font-size: 0.875rem !important;
	color: rgba(0, 0, 0, 0.87) !important;
	padding: 0 16px !important;
	height: 40px !important;
	border-bottom: 1px solid rgba(0, 0, 0, 0.06) !important;
}

.alist-table >>> tbody tr:last-child td {
	border-bottom: none !important;
}

.alist-table >>> tbody tr:hover {
	background: rgba(0, 0, 0, 0.03) !important;
}

.alist-table >>> .v-data-table__wrapper {
	box-shadow: none !important;
}

.alist-empty {
	padding: 24px;
	text-align: center;
	font-size: 0.875rem;
	font-style: italic;
	color: rgba(0, 0, 0, 0.38);
}

/* Severity badges matching Svelte colorMap */
.alist-badge {
	display: inline-flex;
	align-items: center;
	padding: 1px 8px;
	border-radius: 9999px;
	font-size: 0.75rem;
	font-weight: 600;
	border: 2px solid currentColor;
	text-transform: capitalize;
}
.alist-badge--red    { color: #dc2626; }
.alist-badge--yellow { color: #ca8a04; }
.alist-badge--blue   { color: #2563eb; }
.alist-badge--green  { color: #16a34a; }
.alist-badge--slate  { color: rgba(0, 0, 0, 0.45); }
</style>
