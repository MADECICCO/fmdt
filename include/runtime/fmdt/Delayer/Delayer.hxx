#ifndef DELAYER_HXX_
#define DELAYER_HXX_

#include <sstream>
#include <complex>

#include "fmdt/Delayer/Delayer.hpp"

template <typename D>
Delayer<D>
::Delayer(const size_t size, const D init_val)
: Module(), size(size), init_val(init_val), data(this->size * this->n_frames, init_val)
{
	const std::string name = "Delayer";
	this->set_name(name);
	this->set_short_name(name);

	if (size <= 0)
	{
		std::stringstream message;
		message << "'size' has to be greater than 0 ('size' = " << size << ").";
		throw aff3ct::tools::invalid_argument(__FILE__, __LINE__, __func__, message.str());
	}

	auto &p1 = this->create_task("memorize");
	auto p1s_in = this->template create_socket_in <D>(p1, "in", this->size);
	this->create_codelet(p1, [p1s_in](aff3ct::module::Module &m, aff3ct::module::Task &t, const size_t frame_id) -> int
	{
		static_cast<Delayer<D>&>(m)._memorize(static_cast<D*>(t[p1s_in].get_dataptr()), frame_id);
		return 0;
	});

	auto &p2 = this->create_task("produce");
	auto p2s_out = this->template create_socket_out<D>(p2, "out", this->size);
	this->create_codelet(p2, [p2s_out](aff3ct::module::Module &m, aff3ct::module::Task &t, const size_t frame_id) -> int
	{
		static_cast<Delayer<D>&>(m)._produce(static_cast<D*>(t[p2s_out].get_dataptr()), frame_id);
		return 0;
	});
}

template <typename D>
aff3ct::module::Task& Delayer<D>
::operator[](const dly::tsk t) {
	return aff3ct::module::Module::operator[]((int)t);
}

template <typename D>
aff3ct::module::Socket& Delayer<D>
::operator[](const dly::sck::memorize s) {
	return aff3ct::module::Module::operator[]((int)dly::tsk::memorize)[(int)s];
}

template <typename D>
aff3ct::module::Socket& Delayer<D>
::operator[](const dly::sck::produce s) {
	return aff3ct::module::Module::operator[]((int)dly::tsk::produce)[(int)s];
}

template <typename D>
Delayer<D>* Delayer<D>
::clone() const
{
	auto m = new Delayer(*this);
	m->deep_copy(*this);
	return m;
}

template <typename D>
size_t Delayer<D>
::get_size() const
{
	return size;
}

template <typename D>
void Delayer<D>
::_memorize(const D *in, const size_t frame_id)
{
	std::copy(in,
	          in + this->size,
	          this->data.data() + this->size * frame_id);
}

template <typename D>
void Delayer<D>
::_produce(D *out, const size_t frame_id)
{
	std::copy(this->data.data() + this->size * (frame_id +0),
	          this->data.data() + this->size * (frame_id +1),
	          out);
}

template<typename D>
void Delayer<D>
::set_n_frames(const size_t n_frames)
{
	const auto old_n_frames = this->get_n_frames();
	if (old_n_frames != n_frames)
	{
		Module::set_n_frames(n_frames);

		const auto old_data_size = this->data.size();
		const auto new_data_size = (old_data_size / old_n_frames) * n_frames;
		this->data.resize(new_data_size, this->init_val);
	}
}

#endif
