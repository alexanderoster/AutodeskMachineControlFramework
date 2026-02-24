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

	<div class="content-root">

		<div v-if="module.title" class="content-title">{{ module.title }}</div>
		<div v-if="module.subtitle" class="content-subtitle">{{ module.subtitle }}</div>

		<div v-if="module.modules && module.modules.length > 0" class="content-children">
			<Module_Factory
				v-for="childModule in module.modules"
				:key="childModule.uuid"
				:module="childModule"
				:Application="Application"
			/>
		</div>

	</div>

</template>

<script>
export default {
	name: 'Module_Content',
	props: ['Application', 'module'],
};
</script>

<style scoped>
/*
 * Plain block layout. Do NOT use flex or grid here -- the child Grid module
 * relies on width:100% (not flex sizing) to establish its column tracks, and
 * its height:100% must resolve in a block context. Introducing a flex container
 * collapses the grid's height to 0 and breaks the column layout.
 */
.content-root {
	display: block;
	width: 100%;
}

.content-title {
	font-size: 1.125rem;
	font-weight: 600;
	color: rgba(0, 0, 0, 0.75);
	padding: 4px 0 2px;
}

.content-subtitle {
	font-size: 0.9375rem;
	color: rgba(0, 0, 0, 0.54);
	padding-bottom: 8px;
}

.content-children {
	display: block;
	width: 100%;
}
</style>
