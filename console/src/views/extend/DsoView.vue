<script lang="ts" setup>
import { whm_get } from '@/components/Whm.vue';
import { onMounted, ref } from 'vue';
interface Dso {
    name: string,
}
const new_server = ref(false);
const dsos = ref(<Dso[]>[])
function flushDso() {
    whm_get("list_dso").then((json) => {
        dsos.value = json.result.dso;
    });
}
function del_server(dso: Dso) {
    if (!confirm("确定删除 " + dso.name + "吗?")) {
        return;
    }
    whm_get("del_dso", { name: dso.name }).then((json) => {
        flushDso();
    })
}
onMounted(flushDso)
</script>
<template>
    <table border=1 cellspacing="0">
        <tr>
            <th>操作</th>
            <th>名字</th>
            <th>文件</th>
            <th>独立进程</th>
            <th>引用</th>
            <th>状态</th>
            <th>version</th>
            
        </tr>
        <tr v-for="dso in dsos">
            <td>[删除]</td>
            <td>{{ dso.name }}</td>
            <td>文件</td>
            <td>类型</td>
            <td>协议</td>
            <td>端口</td>
            <td>引用</td>
        </tr>
    </table>
</template>