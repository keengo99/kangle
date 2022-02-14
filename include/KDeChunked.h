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
 *  Author: KangHongjiu <keengo99@gmail.com>
 */


#ifndef KDECHUNKED_H_
#define KDECHUNKED_H_
#include "KBuffer.h"
class KHttpTransfer;
#include "KStream.h"
#include "KDechunkEngine.h"
#if 0
class KDeChunked : public KHttpStream{
public:
	KDeChunked(KWStream *st,bool autoDelete);
	virtual ~KDeChunked();
	StreamState write_all(const char *buf,int buf_len);
	//返回是否结束了数据流	
private:
	KDechunkEngine engine;
	//int chunk_size ;
	//int work_len ;
	//char *work;
};
#endif
#endif /* KDECHUNKED_H_ */
