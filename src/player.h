/* (C)opyright 2009 Anton Novikov
 * See LICENSE file for license details.
 */

#ifndef PLAYER_H
#define PLAYER_H

#include <QMutex>
#include <QThread>
#include <vomid.h>

class Player : public QThread
{
	Q_OBJECT

public:
	Player();
	~Player();
	void play(vmd_file_t *, vmd_time_t);
	vmd_file_t *file() const { return file_; }
	vmd_time_t time() const;

public slots:
	void stop();

protected:
	void run();
	void event_clb(unsigned char *, size_t);
	vmd_status_t delay_clb(vmd_time_t, int);

private:
	static void s_event_clb(unsigned char *, size_t, void *);
	static vmd_status_t s_delay_clb(vmd_time_t, int, void *);

	mutable QMutex mutex_;
	vmd_file_t *file_;
	vmd_time_t time_;
	int tempo_;
	double system_time_;
};

#endif /* PLAYER_H */
