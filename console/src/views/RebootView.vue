<script lang="ts" setup>
import { ref } from 'vue';

const result = ref(0);
function reboot() {
    result.value = 1;
    fetch('/core.whm?whm_call=reboot&format=json').then((res) => {
        return res.json();
    }).then((json) => { result.value = json.result; }).catch((e)=>{
       
    });
}
</script>
<template>
    <div v-if="!result">
        确定重起吗?
        <input type="button" @click="reboot()" value="确定" class="btn btn-primary">
    </div>
    <div v-if="result">重起完成,请刷新</div>
</template>