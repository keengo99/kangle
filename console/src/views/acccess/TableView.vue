<script lang="ts" setup>
import { onMounted, ref } from 'vue';
import ChainView, { type Chain } from './ChainView.vue';
const props = defineProps<{ access: string, name: string, vh?: string }>();
const curEditChain = ref(<Chain | null>null);
const chains = ref(<Chain[]|null>null);
function get_whm_url(core: string) {
    let url: string = '/core.whm?whm_call=' + core + '&access=' + props.access + '&format=json'
    if (props.vh) {
        url += '&vh=' + props.vh
    }
    return url
}
function flushChain() {
    fetch(get_whm_url('list_chain') + '&table=' + props.name)
        .then((res) => res.json())
        .then((json) => {
            chains.value = json.result.chain;
            if (chains.value==null) {
                chains.value = [];
            }
        });
}
function editChain(chain: Chain) {
    if (curEditChain.value == chain) {
        curEditChain.value = null;
        return;
    }
    curEditChain.value = chain;
}
function delChain(chain: Chain) {
    if (!confirm("确定要删除吗?")) {
        return;
    }
    fetch(get_whm_url('del_chain') + '&table=' + props.name +
        '&file=' + chain.k.file +
        '&index=' + chain.k.index +
        '&id=' + chain.k.id)
        .then((res) => res.json())
        .then((json) => {
            flushChain();
        });
}
function insertChain(index: number, chain: Chain) {
    let nc: Chain = {
        add: true,
        action: "",
        k: chain.k,
        v: {
            hit: 0
        }
    }
    curEditChain.value = nc
}
function appendChain() {
    curEditChain.value = {
        add: true,
        action: "",
        k: {
            index: 0,
            id: -1,
            file: ""
        },
        v: {
            hit: 0
        }
    }
}
function cancel_edit_chain() {
    if (curEditChain.value == null) {
        return;
    }
    if (curEditChain.value.add) {
        if (chains.value==null) {
            return;
        }
        /* 取消新增,则删除当前的chain */
        for (let i: number = 0; i < chains.value.length; ++i) {
            if (chains.value[i] == curEditChain.value) {
                chains.value.splice(i, 1);
                break;
            }
        }
    }
    curEditChain.value = null;
}
function submit_edit_chain() {
    curEditChain.value = null;
    flushChain();
}
onMounted(flushChain);
</script>
<template>
    <template v-if="chains==null">正在加载</template>
    <template v-else>
    <table border="1" cellspacing="0">
        <tr> 
            <th>操作</th>
            <th>目标</th>
            <th>acl</th>
            <th>mark</th>
            <th>命中</th>
        </tr>
        <template v-for="(chain, index) in chains">
            <tr>
                <td>
                    [<a href=# @click='delChain(chain)'>删</a>]
                    [<a href=# @click='editChain(chain)'>改</a>]
                    [<a href=# @click='insertChain(index, chain)'>插</a>]
                </td>
                <td>{{ chain.action }}<span v-if="chain.jump">:{{ chain.jump }}</span></td>
                <td>
                    <div v-for="m in chain.v.acl">
                        {{ m.module }} {{ m.ref }} <span v-if="m.revers">!</span><span v-html="m.html"></span>
                    </div>
                </td>
                <td>
                    <div v-for="m in chain.v.mark">
                        {{ m.module }} {{ m.ref }}<span v-html="m.html"></span>
                    </div>
                </td>
                <td>{{ chain.v.hit }}</td>
            </tr>                      
        </template>
    </table>   
    <div v-if="curEditChain!=null" class="edit_chain">               
        <ChainView :key="curEditChain.k.file+curEditChain.k.id" :chain='curEditChain' :access="props.access"
                        :table="props.name" :vh="props.vh" @cancel_chain="cancel_edit_chain"
                        @submit_chain="submit_edit_chain" />              
    </div>
    [<a href=# @click='appendChain()'>增加</a>]
    </template>
</template>