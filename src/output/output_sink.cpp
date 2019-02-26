/****
DIAMOND protein aligner
Copyright (C) 2013-2017 Benjamin Buchfink <buchfink@gmail.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
****/

#include "output.h"
#include "../data/queries.h"

using namespace std;

unique_ptr<OutputSink> OutputSink::instance;

void OutputSink::push(size_t n, TextBuffer *buf)
{
	mtx_.lock();
	//cout << "n=" << n << " next=" << next_ << endl;
	if (n != next_) {
		backlog_[n] = buf;
		size_ += buf ? buf->alloc_size() : 0;
		max_size_ = std::max(max_size_, size_);
		mtx_.unlock();
	}
	else
		flush(buf);
}

void OutputSink::flush(TextBuffer *buf)
{
	size_t n = next_ + 1;
	vector<TextBuffer*> out;
	out.push_back(buf);
	map<size_t, TextBuffer*>::iterator i;
	do {
		while ((i = backlog_.begin()) != backlog_.end() && i->first == n) {
			out.push_back(i->second);
			backlog_.erase(i);
			++n;
		}
		mtx_.unlock();
		size_t size = 0;
		for (vector<TextBuffer*>::iterator j = out.begin(); j < out.end(); ++j) {
			if (*j) {
				f_->consume((*j)->get_begin(), (*j)->size());
				if (*j != buf)
					size += (*j)->alloc_size();
				delete *j;
			}
		}
		out.clear();
		mtx_.lock();
		size_ -= size;
	} while ((i = backlog_.begin()) != backlog_.end() && i->first == n);
	next_ = n;
	mtx_.unlock();
}

void heartbeat_worker(size_t qend)
{
	static const int interval = 100;
	int n = 0;
	while (OutputSink::get().next() < qend) {
		if (n == interval) {
			const string title(query_ids::get()[OutputSink::get().next()].c_str());
			verbose_stream << "Queries=" << OutputSink::get().next() << " size=" << megabytes(OutputSink::get().size()) << " max_size=" << megabytes(OutputSink::get().max_size())
				<< " next=" << title.substr(0, title.find(' ')) << endl;
			n = 0;
		}
		else
			++n;
		std::this_thread::sleep_for(chrono::milliseconds(10));
	}
}