<script setup lang="ts">
import { ref } from 'vue';
import FooterView from './FooterView.vue';
const info = ref(<any>{});
fetch('/core.whm?whm_call=info&format=json').then((res) => {
  //console.log(res.json());
  //info.value =  res.json().result;
  return res.json();
}).then((json) => { info.value = json.result; });

</script>

<template>
  <h3>程序信息</h3>
  <table>
    <tr>
      <td>版本</td>
      <td>{{ info.version }}({{ info.type }}) </td>
    </tr>
    <tr>
      <td>路径</td>
      <td>{{ info.kangle_home }}</td>
    </tr>
  </table>
  <div id='version_note'></div>
  <h3>缓存信息</h3>
  <!-- [<a href='#' onClick="if(confirm('sure?')){ window.location='/clean_all_cache'}">清空所有缓存</a>][<a href='#'
    onClick="if(confirm('sure?')){ window.location='/scan_disk_cache.km'}">扫描磁盘缓存</a>][<a
    href='/flush_disk_cache.km'>flush</a>]
-->
  <table>
    <tr>
      <td>缓存总数</td>
      <td colspan=2>{{ info.cache_count }}</td>
    </tr>
    <tr>
      <td>内存缓存</td>
      <td>{{ info.cache_mem_count }}</td>
      <td>{{ info.cache_mem }}</td>
    </tr>
    <tr>
      <td>磁盘缓存</td>
      <td>{{ info.cache_disk_count }}</td>
      <td>{{ info.cache_disk }}</td>
    </tr>
  </table>
  <h3>运行情况</h3>{{ info.total_run }}<h3>负载信息</h3>
  <table>
    <tr>
      <td>连接数</td>
      <td>{{ info.connect }}</td>
    </tr>
    <tr>
      <td>fiber</td>
      <td>{{ info.fiber_count }}</td>
    </tr>
    <tr>
      <td>工作线程数</td>
      <td>{{ info.thread_worker }}</td>
    </tr>
    <tr>
      <td>空闲线程数</td>
      <td>{{ info.thread_free }}</td>
    </tr>
  </table>
  <h3>事件模型</h3>
  <table>
    <tr>
      <td>名字</td>
      <td>{{ info.event_name }} {{ info.fiber_driver }}</td>
    </tr>
    <tr>
      <td>工作线程</td>
      <td>{{ info.event_count }}</td>
    </tr>
  </table>
  <FooterView></FooterView>
</template>
