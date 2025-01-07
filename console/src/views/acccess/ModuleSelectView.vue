<script lang="ts" setup>
import { whm_get } from '@/components/Whm.vue';
import { onMounted, ref } from 'vue';
import type { ModuleBase } from './ModuleView.vue';
const props = defineProps<{
    access:string,
    type:number,
}>()
const emit = defineEmits<{ 
    (e: 'select_module', m: ModuleBase): void
}>();
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
function select_module(e:Event) {
    const target = e.target as HTMLSelectElement;
    if (target.value=="") {
        return;
    }
    whm_get("get_module",{
        access:props.access,
        type:props.type,
        module:target.value
    }).then((json)=>{
        var m:ModuleBase = json.result;
        emit("select_module",m);
    });
}
onMounted(flush_module);
</script>
<template>
    <template v-if="modules!=null">
    <select @change="select_module($event)">
        <option value="">--请选择--</option>
        <template v-for="module in modules">
            <option :value="module">{{ module }}</option>            
        </template>
    </select>
    </template>
</template>