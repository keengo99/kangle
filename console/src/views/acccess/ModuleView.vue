<script lang="ts" setup>
import { computed, onMounted, ref, watchEffect } from 'vue';
export interface ModuleBase {
  module?: string,
  html: string,
}

const props = defineProps<{
  module: ModuleBase
}>()

const getScripts = computed<string[]>(()=>{
  const scriptRegex = /<SCRIPT\b[^>]*>([\s\S]*?)<\/SCRIPT>/g;
  let match;
  let scripts = <string[]>[];
  while ((match = scriptRegex.exec(props.module.html)) !== null) {
    scripts.push(match[1]);
    // console.log("js=[" + match[1] +"]")
  }
  return scripts;
})
const getCleanHtml = computed<string>(()=>{
  const scriptRegex2 = /<SCRIPT\b[^>]*>([\s\S]*?)<\/SCRIPT>/g;
  return props.module.html.replace(scriptRegex2, '')
})
</script>
<template>
  <component v-for="js in getScripts" is="script" v-html="js">
  </component>
  <div v-html="getCleanHtml">
  </div>
</template>
