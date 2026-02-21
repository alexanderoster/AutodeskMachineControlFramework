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
<div v-if="moduleitem.type === 'buildlist'">

  <v-data-table
    :headers="tableHeaders"
    :items="moduleitem.entries"
    :items-per-page="moduleitem.entriesperpage"
    class="buildlist-table elevation-1"
    :loading-text="moduleitem.loadingtext"
    @click:row="onRowClick"
    style="cursor: pointer;"
  >

    <!-- Thumbnail -->
    <template v-slot:[`item.buildThumbnail`]="{ item }">
      <v-img
        v-if="hasThumbnail(item)"
        height="56"
        width="80"
        contain
        :src="Application.getImageURL(item.buildThumbnail)"
        class="rounded buildlist-thumb"
      />
      <div v-else class="buildlist-no-thumb">
        <v-icon color="grey lighten-1" size="32">mdi-file-3d-outline</v-icon>
      </div>
    </template>

    <!-- Build name + execution count subtitle -->
    <template v-slot:[`item.buildName`]="{ item }">
      <div class="buildlist-name-cell">
        <span class="buildlist-name-text">{{ item.buildName }}</span>
        <span class="buildlist-sub-text">
          {{ item.buildExecutionCount === 1 ? 'Printed 1 time' : 'Printed ' + item.buildExecutionCount + ' times' }}
        </span>
      </div>
    </template>

    <!-- Layer count -->
    <template v-slot:[`item.buildLayers`]="{ item }">
      <span class="buildlist-meta-text">{{ item.buildLayers }}</span>
    </template>

    <!-- Upload time + user -->
    <template v-slot:[`item.buildTimestamp`]="{ item }">
      <div class="buildlist-date-cell">
        <span class="buildlist-date-text">{{ formatDateTime(item.buildTimestamp) }}</span>
        <span class="buildlist-sub-text">{{ item.buildUser }}</span>
      </div>
    </template>

    <!-- UUID -->
    <template v-slot:[`item.buildUUID`]="{ item }">
      <span class="buildlist-uuid-text">{{ item.buildUUID }}</span>
    </template>

    <!-- Status badge -->
    <template v-slot:[`item.buildStatus`]>
      <div class="buildlist-status-cell">
        <v-icon small color="green darken-1" class="mr-1">mdi-check-circle</v-icon>
        <span class="buildlist-status-ready">Ready</span>
      </div>
    </template>

    <!-- Extra action buttons (optional) -->
    <template v-slot:[`item.actions`]="{ item }">
      <div class="buildlist-actions-cell">
        <v-btn
          v-for="button in moduleitem.entrybuttons"
          :key="button.uuid"
          text
          x-small
          color="primary"
          @click.stop="uiModuleBuildListHistoryClick(button, item)"
        >
          <v-icon v-if="button.icon" small left>{{ button.icon }}</v-icon>
          {{ button.caption }}
        </v-btn>
      </div>
    </template>

    <!-- Empty state -->
    <template v-slot:no-data>
      <div class="buildlist-empty-state">
        <v-icon color="grey lighten-1" size="48">mdi-folder-open-outline</v-icon>
        <p class="buildlist-empty-title">No build files uploaded</p>
        <p class="buildlist-empty-hint">Click on a row to view details and select a build</p>
      </div>
    </template>

  </v-data-table>

  <!-- Detail dialog -->
  <v-dialog v-model="showDialog" max-width="540" @click:outside="showDialog = false">
    <v-card v-if="selectedEntry">
      <v-card-title class="buildlist-dialog-title">
        <v-icon left color="primary">mdi-file-3d-outline</v-icon>
        Build details
        <v-spacer />
        <v-btn icon small @click="showDialog = false">
          <v-icon>mdi-close</v-icon>
        </v-btn>
      </v-card-title>
      <v-divider />

      <v-card-text class="buildlist-dialog-body pt-4">

        <div v-if="hasThumbnail(selectedEntry)" class="buildlist-dialog-thumb-row mb-4">
          <v-img
            :src="Application.getImageURL(selectedEntry.buildThumbnail)"
            max-height="160"
            contain
            class="rounded"
          />
        </div>

        <div class="buildlist-dialog-row">
          <span class="buildlist-dialog-label">File name</span>
          <span class="buildlist-dialog-value font-weight-medium">{{ selectedEntry.buildName }}</span>
        </div>
        <div class="buildlist-dialog-row">
          <span class="buildlist-dialog-label">Layers</span>
          <span class="buildlist-dialog-value">{{ selectedEntry.buildLayers }}</span>
        </div>
        <div class="buildlist-dialog-row">
          <span class="buildlist-dialog-label">Uploaded by</span>
          <span class="buildlist-dialog-value">{{ selectedEntry.buildUser }}</span>
        </div>
        <div class="buildlist-dialog-row">
          <span class="buildlist-dialog-label">Upload time</span>
          <span class="buildlist-dialog-value">{{ formatDateTime(selectedEntry.buildTimestamp) }}</span>
        </div>
        <div class="buildlist-dialog-row">
          <span class="buildlist-dialog-label">Executions</span>
          <span class="buildlist-dialog-value">{{ selectedEntry.buildExecutionCount }}</span>
        </div>
        <div class="buildlist-dialog-row buildlist-dialog-uuid-row">
          <span class="buildlist-dialog-label">UUID</span>
          <span class="buildlist-uuid-text">{{ selectedEntry.buildUUID }}</span>
        </div>

      </v-card-text>

      <v-divider />
      <v-card-actions>
        <v-spacer />
        <v-btn text @click="showDialog = false">Cancel</v-btn>
        <v-btn color="primary" @click="onSelectFromDialog">
          <v-icon left small>mdi-check</v-icon>
          Select for execution
        </v-btn>
      </v-card-actions>
    </v-card>
  </v-dialog>

</div>
</template>

<script>
export default {
  props: ['Application', 'moduleitem'],

  data() {
    return {
      showDialog: false,
      selectedEntry: null,
    };
  },

  computed: {
    tableHeaders() {
      const headers = [
        { text: '', value: 'buildThumbnail', sortable: false, width: '96px' },
        { text: 'Build', value: 'buildName', sortable: true },
        { text: 'Layers', value: 'buildLayers', sortable: true, width: '80px' },
        { text: 'Uploaded', value: 'buildTimestamp', sortable: true },
        { text: 'UUID', value: 'buildUUID', sortable: false },
        { text: 'Status', value: 'buildStatus', sortable: false, width: '110px' },
      ];
      if (this.moduleitem.entrybuttons && this.moduleitem.entrybuttons.length > 0) {
        headers.push({ text: '', value: 'actions', sortable: false, width: '1px' });
      }
      return headers;
    },
  },

  methods: {
    hasThumbnail(item) {
      return item.buildThumbnail &&
             item.buildThumbnail !== '00000000-0000-0000-0000-000000000000';
    },

    formatDateTime(timeString) {
      if (!timeString) return '';
      const date = new Date(timeString);
      if (isNaN(date.getTime())) return timeString;
      return new Intl.DateTimeFormat('en-US', {
        year: 'numeric',
        month: 'short',
        day: 'numeric',
        hour: '2-digit',
        minute: '2-digit',
        second: '2-digit',
        hour12: false,
      }).format(date);
    },

    onRowClick(item) {
      this.selectedEntry = item;
      this.showDialog = true;
    },

    onSelectFromDialog() {
      this.showDialog = false;
      if (this.selectedEntry && this.moduleitem.selectevent && this.moduleitem.selectionvalueuuid) {
        const eventValues = {};
        eventValues[this.moduleitem.selectionvalueuuid] = this.selectedEntry.buildUUID;
        this.Application.triggerUIEvent(this.moduleitem.selectevent, this.moduleitem.uuid, eventValues);
      }
    },

    uiModuleBuildListHistoryClick(button, item) {
      if (item && button && button.selectevent &&
          this.moduleitem.selectionvalueuuid && this.moduleitem.buttonvalueuuid) {
        const eventValues = {};
        eventValues[this.moduleitem.selectionvalueuuid] = item.buildUUID;
        eventValues[this.moduleitem.buttonvalueuuid] = button.uuid;
        this.Application.triggerUIEvent(button.selectevent, this.moduleitem.uuid, eventValues);
      }
    },
  },
};
</script>

<style scoped>
/* Table */
.buildlist-table {
  border-radius: 6px;
}
.buildlist-table >>> tbody tr:hover {
  background-color: rgba(0, 0, 0, 0.04) !important;
}

/* Thumbnail cell */
.buildlist-thumb {
  border-radius: 4px;
  margin: 4px 0;
}
.buildlist-no-thumb {
  width: 80px;
  height: 56px;
  display: flex;
  align-items: center;
  justify-content: center;
  background: #f5f5f5;
  border-radius: 4px;
  margin: 4px 0;
}

/* Name cell */
.buildlist-name-cell {
  display: flex;
  flex-direction: column;
  gap: 2px;
  padding: 6px 0;
}
.buildlist-name-text {
  font-weight: 500;
  font-size: 0.875rem;
  line-height: 1.3;
}
.buildlist-sub-text {
  font-size: 0.75rem;
  color: rgba(0, 0, 0, 0.45);
  font-style: italic;
}

/* Date cell */
.buildlist-date-cell {
  display: flex;
  flex-direction: column;
  gap: 2px;
}
.buildlist-date-text {
  font-size: 0.8125rem;
}

/* UUID */
.buildlist-uuid-text {
  font-size: 0.7rem;
  color: rgba(0, 0, 0, 0.38);
  font-family: monospace;
  letter-spacing: 0.01em;
}

/* Layer count */
.buildlist-meta-text {
  font-size: 0.8125rem;
  color: rgba(0, 0, 0, 0.55);
  font-style: italic;
}

/* Status */
.buildlist-status-cell {
  display: flex;
  align-items: center;
  gap: 4px;
}
.buildlist-status-ready {
  font-size: 0.8125rem;
  color: #2e7d32;
  font-weight: 500;
}

/* Action buttons */
.buildlist-actions-cell {
  display: flex;
  gap: 2px;
  white-space: nowrap;
}

/* Empty state */
.buildlist-empty-state {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 6px;
  padding: 40px 16px;
}
.buildlist-empty-title {
  font-size: 0.9375rem;
  color: rgba(0, 0, 0, 0.45);
  margin: 0;
}
.buildlist-empty-hint {
  font-size: 0.8125rem;
  color: rgba(0, 0, 0, 0.3);
  font-style: italic;
  margin: 0;
}

/* Detail dialog */
.buildlist-dialog-title {
  font-size: 1rem !important;
  padding: 14px 20px !important;
}
.buildlist-dialog-body {
  padding: 16px 20px !important;
}
.buildlist-dialog-thumb-row {
  border-radius: 6px;
  overflow: hidden;
  background: #f5f5f5;
}
.buildlist-dialog-row {
  display: flex;
  align-items: baseline;
  gap: 12px;
  padding: 5px 0;
  border-bottom: 1px solid rgba(0, 0, 0, 0.06);
}
.buildlist-dialog-row:last-child {
  border-bottom: none;
}
.buildlist-dialog-label {
  font-size: 0.8125rem;
  font-weight: 500;
  color: rgba(0, 0, 0, 0.55);
  min-width: 140px;
  flex-shrink: 0;
}
.buildlist-dialog-value {
  font-size: 0.875rem;
}
.buildlist-dialog-uuid-row .buildlist-uuid-text {
  word-break: break-all;
}
</style>