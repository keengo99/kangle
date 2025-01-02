<script lang="ts" setup>
import type { ServerNode } from '@/components/ServerNode.vue';
import { whm_get } from '@/components/Whm.vue';
import { onMounted, ref } from 'vue';
interface Sserver extends ServerNode {
    name: string,
}
const new_server = ref(false);
const sservers = ref(<Sserver[]>[])
function flushSserver() {
    whm_get("list_sserver").then((json) => {
        sservers.value = json.result.sserver;
    });
}
function del_server(server: Sserver) {
    if (!confirm("确定删除 " + server.name + "吗?")) {
        return;
    }
    whm_get("del_sserver", { name: server.name }).then((json) => {
        flushSserver();
    })
}
onMounted(flushSserver)
</script>
<template>
    <table border="1" cellspacing="0">
        <tr>
            <td>操作</td>
            <td>名称</td>
            <td>地址</td>
            <td>端口</td>
            <td>协议</td>
        </tr>
        <tr v-for="server in sservers">
            <td>[<a href=# @click="del_server(server)">删除</a>]</td>
            <td>{{ server.name }}</td>
            <td>{{ server.host }}</td>
            <td>{{ server.port }}</td>
            <td>{{ server.proto }}</td>
        </tr>
    </table>
    [<a href=# @click="new_server = !new_server">新增</a>]
    <div v-if="new_server">        
            <div>名字:<input name=name value=''></div>
            <div>协议:<input type='radio' name='proto' value='http' checked>http<input
                type='radio' name='proto' value='fastcgi'>fastcgi <input type='radio' value='ajp' name='proto'>ajp<input
                type='radio' name='proto' value='tcp'>tcp<input type='radio' name='proto'
                value='proxy'>proxy</div>
            <div>主机地址<input name='host' value=''>端口<input size=5 name=port value=''></div>
            <div>生存时间<input name='life_time' size=5 value=0>(设置0表示不启用长连接)</div>
            <div>param:<input name='param' value=''></div>
            <div><input type=submit value=提交></div>

    </div>

</template>