#include <QAction>
#include <QThread>
#include <QTimer>
#include "player.h"

Player::Player()
	:file_(NULL),
	stopping_(false)
{
	connect(this, SIGNAL(finished()), this, SLOT(stop()));
}

Player::~Player()
{
	stop();
}

vmd_time_t
Player::time() const
{
	if (file_ == NULL)
		return -1;

	QMutexLocker locker(&mutex_);
	double dsec = vmd_systime() - system_time_;
	vmd_time_t dt = vmd_systime2time(dsec, tempo_, file_->division);
	return time_ + dt;
}

void
Player::play(vmd_file_t *_file, vmd_time_t _time)
{
	stop();
	file_ = _file;
	time_ = _time;
	tempo_ = vmd_map_get(&file_->ctrl[VMD_FCTRL_TEMPO], time_, NULL);
	system_time_ = vmd_systime();
	start();
}

void
Player::stop()
{
	stop_mutex_.lock();
	stopping_ = true;
	stop_cond_.wakeAll();
	stop_mutex_.unlock();
	wait();
	stopping_ = false;
	file_ = NULL;
}

bool
Player::set_output_device(QString id)
{
	if (vmd_set_device(VMD_OUTPUT_DEVICE, id.toLatin1().data()) == VMD_OK) {
		emit outputDeviceSet(id);
		return true;
	} else
		return false;
}

struct Playing
{
	vmd_file_t *file;
	vmd_time_t time;
	int tempo;
	double system_time;
};

void
Player::run()
{
	QMutexLocker locker(&stop_mutex_);
	vmd_reset_output();
	vmd_file_play(file_, time_, s_event_clb, s_delay_clb, this, NULL);
	vmd_notes_off();
}

void
Player::event_clb(unsigned char *ev, size_t size)
{
	vmd_output(ev, size);
}

vmd_status_t
Player::delay_clb(vmd_time_t dtime, int _tempo)
{
	vmd_flush_output();

	double fin_time = system_time_ + vmd_time2systime(dtime, _tempo, file_->division);

	int msec_wait = int((fin_time - vmd_systime()) * 1000);
	if (msec_wait < 0)
		msec_wait = 0;
	if (stop_cond_.wait(&stop_mutex_, msec_wait) || stopping_)
		return VMD_STOP;

	QMutexLocker lock(&mutex_);
	time_ += dtime;
	system_time_ = fin_time;
	tempo_ = _tempo;
	return VMD_OK;
}

void
Player::s_event_clb(unsigned char *b, size_t s, void *p)
{
	static_cast<Player *>(p)->event_clb(b, s);
}

vmd_status_t
Player::s_delay_clb(vmd_time_t t, int tempo, void *p)
{
	return static_cast<Player *>(p)->delay_clb(t, tempo);
}
