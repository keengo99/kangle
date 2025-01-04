import { createRouter, createWebHistory } from 'vue-router'
import InfoView from '../views/InfoView.vue'

const router = createRouter({
  history: createWebHistory(import.meta.env.BASE_URL),
  routes: [
    {
      path: '/',
      name: 'home',
      component: InfoView,
    },
    {
      path: '/index.html',
      name: 'home2',
      component: InfoView,
    },
    {
      path: '/request',
      name: 'request',
      component: () => import('../views/RequestView.vue'),
    },
    {
      path: '/response',
      name: 'response',
      component: () => import('../views/ResponseView.vue'),
    },
    {
      path: '/process',
      name: 'process',
      component: () => import('../views/ProcessView.vue'),
    },
    {
      path: '/virtualhost',
      name: 'virtualhost',
      component: () => import("../views/VirtualHostView.vue"),
    },
    {
      path: '/connect_per_ip',
      name: 'connect_per_ip',
      component: () => import('../views/ConnectPerIpView.vue'),
    },
    {
      path: '/connection',
      name: 'connection',
      component: () => import('../views/ConnectionView.vue'),
    },
    {
      path: '/config',
      name: 'config',
      component: () => import('../views/ConfigView.vue'),
      children :[
        {
          path: 'listen',
          name: 'listen',
          component: ()=>import('../views/config/ListenView.vue'),
        },
        {
          path: 'cache',
          name: 'cache',
          component: ()=>import('../views/config/CacheView.vue'),
        }
      ]
    },
    {
      path: '/reboot',
      name: 'reboot',
      component: () => import('../views/RebootView.vue'),
    },
    {
      path: '/extend',
      name: 'extend',
      component: () => import("../views/ExtendView.vue"),
      children: [
        {
          path: 'sserver',
          name: 'sserver',
          component: () => import("../views/extend/SserverView.vue"),
        },
        {
          path: 'mserver',
          name: 'mserver',
          component: () => import("../views/extend/MServerView.vue"),
        },
        {
          path: 'cmd',
          name: 'cmd',
          component: () => import("../views/extend/CmdView.vue"),
        },
        {
          path: 'api',
          name: 'api',
          component: () => import("../views/extend/ApiView.vue"),
        },
        {
          path: 'dso',
          name: 'dso',
          component: () => import("../views/extend/DsoView.vue"),
        },
      ],
    }
  ],
})

export default router
