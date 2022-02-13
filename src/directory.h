/*
 * Copyright (c) 2010, NanChang BangTeng Inc
 *
 * kangle web server              http://www.kangleweb.net/
 * ---------------------------------------------------------------------
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.
 *  See COPYING file for detail.
 *
 *  Author: KangHongjiu <keengo99@gmail.com>  2011-07-18
 */
#ifndef DIRECTORY_H
#define DIRECTORY_H
//返回0正确，其它失败
typedef int (* list_dir_handle_f )(const char *file,void *param);
typedef int (* list_dir_handlew_f )(const wchar_t *file,void *param);
//返回0正确，其它失败
int list_dir(const char *dir,list_dir_handle_f file_handle,void *param);
int list_dirw(const wchar_t *dir,list_dir_handlew_f file_handle,void *param);

#endif
