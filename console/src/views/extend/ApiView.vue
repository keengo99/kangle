<script lang="ts" setup>
import { whm_get } from '@/components/Whm.vue';
import { onMounted, ref } from 'vue';
interface Api {
    name: string,
    file: string,
    type: number,
    state:string,
    info: string,
    refs:number
}
const new_server = ref(false);
const apis = ref(<Api[]>[])
function flushApi() {
    whm_get("list_api").then((json) => {
        apis.value = json.result.api;
    });
}
function del_api(api: Api) {
    if (!confirm("确定删除 " + api.name + "吗?")) {
        return;
    }
    whm_get("del_api", { name: api.name }).then((json) => {
        flushApi();
    })
}
onMounted(flushApi)
</script>
<template>
    <table border=1 cellspacing="0">
        <tr>
            <th>操作</th>
            <th>名字</th>
            <th>文件</th>
            <th>独立进程</th>
            <th>状态</th>
            <th>版本</th>
            <th>引用</th>
        </tr>
        <tr v-for="api in apis">
            <td>[<a href=# @click="del_api(api)">删除</a>]</td>
            <td>{{ api.name }}</td>
            <td>{{ api.file }}</td>
            <td>{{ api.type==3?"否":"是" }}</td>
            <td>{{ api.state }}</td>
            <td>{{ api.info }}</td>
            <td>{{ api.refs }}</td>
        </tr>
    </table>
</template>