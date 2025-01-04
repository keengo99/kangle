<script lang="ts" setup>
import { whm_get } from '@/components/Whm.vue';
import { onMounted, ref } from 'vue';
interface Cmd {
    name: string,
    refs:number
}
const new_server = ref(false);
const cmds = ref(<Cmd[]>[])
function flushCmd() {
    whm_get("list_cmd").then((json) => {
        cmds.value = json.result.cmd;
    });
}
function del_cmd(cmd: Cmd) {
    if (!confirm("确定删除 " + cmd.name + "吗?")) {
        return;
    }
    whm_get("del_cmd", { name: cmd.name }).then((json) => {
        flushCmd();
    })
}
onMounted(flushCmd)
</script>
<template>
    <table border=1 cellspacing="0">
        <tr>
            <th>操作</th>
            <th>名字</th>
            <th>文件</th>
            <th>类型</th>
            <th>协议</th>
            <th>端口</th>
            <th>引用</th>
            <th>环境变量</th>
        </tr>
        <tr v-for="cmd in cmds">
            <td>[<a href=# @click="del_cmd(cmd)">删除</a>]</td>
            <td>{{ cmd.name }}</td>
            <td>文件</td>
            <td>类型</td>
            <td>协议</td>
            <td>端口</td>
            <td>{{ cmd.refs }}</td>
            <td>环境变量</td>
        </tr>
    </table>
</template>