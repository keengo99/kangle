<script lang="ts" setup>
import { whm_get, whm_post } from '@/components/Whm.vue';
import { onMounted, ref } from 'vue';

interface ConfigItem {
    default: number,
    memory: string,
    disk: string,
    disk_dir: string,
    disk_work_time: string,
    max_cache_size: string,
    max_bigobj_size: string,
    cache_part: number,
    refresh_time: number,
}
const configItem = ref<ConfigItem | null>(null);
function submitConfig() {
    const form = document.getElementById('form') as HTMLFormElement;
    const formData = new FormData(form);
    formData.append('item', '1');
    whm_post('config_submit', formData).then((json) => {
        flushConfig();
    });
}
function flushConfig() {
    whm_get('config', { item: 1 }).then((json) => {
        configItem.value = json.result;
    });
}
onMounted(() => {
    flushConfig();
});
</script>
<template>
    <div v-if="configItem">
        <form onsubmit="return false;" id="form">
            <div>默认缓存<select name="default">
                <option value="1" :selected="configItem.default==1"> 是</option>
                <option value="0" :selected="configItem.default==0">否</option>
            </select></div>
            <div>
            内存缓存:<input type="text" name="memory" size="8" :value="configItem.memory"></div>
            <div>磁盘缓存:<input type="text" name="disk" size="8" :value="configItem.disk"></div>
            <div>磁盘缓存目录:<input type="text" name="disk_dir" :value="configItem.disk_dir">[<a href="/format_disk_cache_dir.km">格式化磁盘缓存目录</a>]</div>
            <div>磁盘扫描时间:<input type="text" name="disk_work_time" :value="configItem.disk_work_time"></div>
            <div>最大缓存网页(普通):<input type="text" name="max_cache_size" size="6" :value="configItem.max_cache_size"></div>
            <div>最大缓存网页(智能):<input type="text" name="max_bigobj_size" size="6" :value="configItem.max_bigobj_size"></div>
            <div>cache part:<input type="checkbox" name="cache_part" value="1" :checked="configItem.cache_part!=0"></div>
            <div>默认缓存时间:<input type="text" name="refresh_time" size="4" :value="configItem.refresh_time">秒</div>
            <div><input type="button" value="提交" @click="submitConfig"></div>
        </form>
    </div>
</template>