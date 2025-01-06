<script lang="ts" setup>
import { whm_get } from '@/components/Whm.vue';
import { onMounted, ref } from 'vue';
const props = defineProps<{
    access:string,
    type:number,
}>()

const modules = ref(<string[]|null>null);
function flush_module() {
    let param:Record<string,any> = {
        access:props.access,
        type:props.type
    }
    whm_get("list_module",param).then((json)=>{
        modules.value = json.result.module;
    })
}
onMounted(flush_module);
</script>
<template>
    <select>
        <option>--请选择--</option>
        <template v-for="module in modules">
            <option value="{{ module }}">{{ module }}</option>            
        </template>
    </select>
</template>