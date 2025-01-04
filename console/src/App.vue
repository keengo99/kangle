<script setup lang="ts">
import { RouterLink, RouterView } from 'vue-router'
</script>

<template>
  <table width="100%">
    <tr>
      <td valign="top" width="160px;">        
          <div id="pmalogo">
            <img id="imgpmalogo" src="./assets/logo.gif" alt="kangle">
          </div>
          <table width='100%'>
            <tr>
              <td height=30>
                <RouterLink to='/'>总体信息</RouterLink>
              </td>
            </tr>
            <tr>
              <td height=30><RouterLink to='/request'>请求控制</RouterLink></td>
            </tr>
            <tr>
              <td height=30><RouterLink to='/response'>回应控制</RouterLink></td>
            </tr>
            <tr>
              <td height=30><RouterLink to='/extend/sserver'>扩展</RouterLink></td>
            </tr>
            <tr>
              <td height=30><RouterLink to='/virtualhost'>虚拟主机</RouterLink></td>
            </tr>
            <tr>
              <td height=30><RouterLink to='/process'>子进程</RouterLink></td>
            </tr>
            <tr>
              <td height=30><RouterLink to='/connect_per_ip'>每IP连接</RouterLink></td>
            </tr>
            <tr>
              <td height=30><RouterLink to='/connection'>连接信息</RouterLink></td>
            </tr>
            <tr>
              <td height=30><RouterLink to='/config/listen'>配置</RouterLink></td>
            </tr>
            <tr>
              <td height=30><RouterLink to='/reboot'>重起</RouterLink></td>
            </tr>
            <tr>
              <td>
                <a href="/old">旧版</a>
              </td>
            </tr>
          </table>     
      </td>
      <td valign="top">
        <RouterView />
      </td>
    </tr>
  </table>
</template>
