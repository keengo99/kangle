<script lang="ts" setup>
import { onMounted, ref } from 'vue';
import TableView from './TableView.vue'
import DefaultAction from './ActionView.vue';

const props = defineProps<{ access: string, vh?: string }>();
interface TableView {
    name: string,
    refs: number,
    show_chain?: boolean,
};
const tables = ref(<TableView[] | null>null);
function get_whm_url(call_name: string) {
    let url: string = '/core.whm?format=json&whm_call=' + call_name + '&access=' + props.access;
    if (props.vh) {
        url += '&vh=' + props.vh
    }
    return url
}
function flush_table() {
    fetch(get_whm_url('list_table')).then((res) => res.json()).then((json) => {
        tables.value = json.result.table;
        if (tables.value == null) {
            tables.value = [];
        }
    });
}
onMounted(flush_table)
function toggle_chain(table: TableView) {
    table.show_chain = !table.show_chain;
}
function del_table(table: TableView) {
    if (confirm('您确定要删除表吗？')) {
        fetch(get_whm_url('del_table') + '&name=' + table.name).then((res) => res.json()).then((json) => {
            flush_table();
        });
    }
}
function new_table() {
    let table = prompt("输入表的名字");
    if (table == null) {
        return;
    }
    fetch(get_whm_url('add_table') + '&name=' + table).then((res) => res.json()).then((json) => {
        flush_table();
    });
}
</script>
<template>
    [<RouterLink to='/named_module'>命名模块</RouterLink>]
    <DefaultAction :access=props.access></DefaultAction>
    [<a @click="new_table" href=#>新建表</a>]
    <div v-if="tables == null">正在加载</div>
    <template v-else>
        <div v-for="table in tables">
            <a href=# @click="toggle_chain(table)">{{ table.name }}</a> 引用:{{ table.refs }}
            [<a href=# @click="del_table(table)">删除</a>]
            <div v-if="table.show_chain">
                <TableView :access=props.access :name=table.name :vh=props.vh />
            </div>
        </div>
    </template>
</template>
