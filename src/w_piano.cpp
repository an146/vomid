#include <QKeyEvent>
#include <QInputDialog>
#include <QPainter>
#include <QPaintEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QToolTip>
#include "file.h"
#include "player.h"
#include "w_main.h"
#include "w_piano.h"

#ifdef Q_WS_WIN
#include <windows.h>
#endif

const int margin = 50;
const int level_height = 5;
const int scroll_margin = 5;
const int quarter_width = 50;
const int update_freq = 10;

static vmd_time_t
grid_snap_left(WPiano *piano, vmd_time_t time)
{
	if (time < 0)
		return 0;
	vmd_measure_t measure;
	vmd_file_measure_at(piano->file(), time, &measure);

	vmd_time_t ret = measure.beg;
	while (ret + piano->grid_size() <= time)
		ret += piano->grid_size();
	return ret;
}

static vmd_time_t
grid_snap_right(WPiano *piano, vmd_time_t time)
{
	if (time < 0)
		return 0;
	vmd_measure_t measure;
	vmd_file_measure_at(piano->file(), time, &measure);

	vmd_time_t ret = measure.beg;
	while (ret <= time)
		ret += piano->grid_size();
	return std::min(ret, measure.end);
}

static int
pitch2level(const vmd_track_t *track, vmd_pitch_t p)
{
	const vmd_notesystem_t *ns = &track->notesystem;
	int octaves = p / ns->size;
	p %= ns->size;
	return octaves * (vmd_notesystem_levels(ns) + 1) + vmd_notesystem_pitch2level(ns, p) + 1;
}

static int
levels(const vmd_track_t *track)
{
	return pitch2level(track, track->notesystem.end_pitch);
}

static vmd_pitch_t
level2pitch(const vmd_track_t *track, int level)
{
	if (level < 0 || level >= levels(track))
		return -1;
	const vmd_notesystem_t *ns = &track->notesystem;
	int octaves = level / (vmd_notesystem_levels(ns) + 1);
	level %= (vmd_notesystem_levels(ns) + 1);
	if (level == 0)
		return -1;
	vmd_pitch_t octave_pitch = vmd_notesystem_level2pitch(ns, level - 1);
	if (octave_pitch < 0)
		return -1;
	return octaves * ns->size + octave_pitch;
}

struct avg_note_arg {
	vmd_track_t *track;
	int min;
	int max;
};

static void *
avg_note_clb(vmd_note_t *note, void *_arg)
{
	avg_note_arg *arg = (avg_note_arg *)_arg;
	int level = pitch2level(note->track, note->pitch);
	arg->min = std::min(arg->min, level);
	arg->max = std::max(arg->max, level);
	return NULL;
}

static int
avg_level(vmd_track_t *track, vmd_time_t beg, vmd_time_t end)
{
	avg_note_arg arg = {
		track,
		levels(track),
		0
	};
	vmd_track_range(track, beg, end, avg_note_clb, &arg);
	return (arg.min + arg.max) / 2;
}

WPiano::WPiano(File *_file, vmd_track_t *_track, vmd_time_t time, Player *_player)
	:file_(_file),
	track_(_track),
	grid_size_(file()->division),
	mouse_captured_(false),
	cursor_time_(grid_snap_left(this, time)),
	cursor_size_(file()->division),
	cursor_level_(0),
	player_(_player)
{
	int w = time2x(vmd_file_length(file()));
	int h = margin * 2 + level_height * levels(track());
	setMinimumSize(w, h);
	setBackgroundRole(QPalette::Base);
	setAutoFillBackground(true);
	setFocusPolicy(Qt::StrongFocus);

	connect(file_, SIGNAL(acted()), this, SLOT(update()));
	connect(player_, SIGNAL(started()), this, SLOT(playStarted()));
	connect(player_, SIGNAL(finished()), this, SLOT(playStopped()));
	if (playing())
		playStarted();
}

QRect
WPiano::viewport() const {
	QWidget *w = qobject_cast<QWidget *>(parent());
	if (w == NULL)
		w = const_cast<WPiano *>(this);
	QRect geom = w->geometry();
	return QRect(mapFrom(w, geom.topLeft()), mapFrom(w, geom.bottomRight()));
}

vmd_time_t
WPiano::x2time(int x) const
{
	return x * (signed)file()->division / quarter_width;
}

int
WPiano::time2x(vmd_time_t time) const
{
	return time * quarter_width / (signed)file()->division;
}

vmd_time_t
WPiano::l_time() const
{
	return x2time(viewport().left());
}

vmd_time_t
WPiano::r_time() const
{
	return x2time(viewport().right());
}

int
WPiano::y2level(int y) const
{
	float flevel = float(height() - margin - y) / level_height;
	return int(flevel + 0.5f);
}

int
WPiano::level2y(int level) const
{
	return height() - (margin + level_height * level);
}

vmd_pitch_t
WPiano::cursor_pitch() const
{
	return level2pitch(track(), cursor_level_);
}

QRect
WPiano::cursor_rect() const
{
	if (playing())
		return QRect(time2x(cursor_time_), 0, 1, height());

	int x1 = time2x(cursor_time_);
	int x2 = time2x(cursor_time_ + cursor_size_);
	int y1 = level2y(cursor_level_ + 1);
	int y2 = level2y(cursor_level_ - 1);
	return QRect(x1, y1, x2 - x1, y2 - y1);
}

void
WPiano::setCursorPos(vmd_time_t time, int level)
{
	if (time != cursor_time_ || level != cursor_level_) {
		update(cursor_rect().adjusted(-3, -3, 3, 3));
		cursor_time_ = time;
		cursor_level_ = level;
		update(cursor_rect().adjusted(-3, -3, 3, 3));
		emit cursorMoved();
	}
}

static void *
pitch_filter(vmd_note_t *note, void *pitch)
{
	if (note->pitch == *(vmd_pitch_t *)pitch)
		return note;
	else
		return NULL;
}

vmd_note_t *
WPiano::noteAtCursor()
{
	vmd_pitch_t pitch = cursor_pitch();
	if (pitch < 0)
		return NULL;
	return (vmd_note_t *)vmd_track_range(track(), cursor_l(), cursor_r(), pitch_filter, &pitch);
}

void
WPiano::focusOutEvent(QFocusEvent *)
{
	capture_mouse(false);
}

void
WPiano::keyPressEvent(QKeyEvent *ev)
{
	int key = ev->key();
	int mod = ev->modifiers() & (Qt::CTRL | Qt::SHIFT);

	switch (key) {
	case Qt::Key_Escape:
		capture_mouse(false);
		break;
	case Qt::Key_Left:
		setCursorTime(grid_snap_left(this, cursor_time_ - 1));
		break;
	case Qt::Key_Right:
		setCursorTime(grid_snap_right(this, cursor_time_ + 1));
		break;
	case Qt::Key_Up:
		if (cursor_level_ < levels(track()))
			setCursorLevel(cursor_level_ + 1);
		break;
	case Qt::Key_Down:
		if (cursor_level_ > 0)
			setCursorLevel(cursor_level_ - 1);
		break;
	case Qt::Key_Space:
		if (playing())
			player_->stop();
		else
			player_->play(file(), cursor_time_);
		break;
	case Qt::Key_QuoteLeft:
		{
			QString s = QInputDialog::getText(
				this,
				"vomid",
				mod & Qt::CTRL ? QString("Enter grid size:") : QString("Enter cursor size:")
			);
			QStringList sl = s.split("/");
			if (sl.size() != 2)
				break;
			int n = sl[0].toInt();
			int m = sl[1].toInt();
			if (n <= 0 && m <= 0)
				break;
			vmd_time_t size = file()->division * 4 * n / m;
			if (mod & Qt::CTRL)
				grid_size_ = size;
			else
				cursor_size_ = size;
		}
		break;
	case Qt::Key_1: case Qt::Key_2: case Qt::Key_3: case Qt::Key_4:
	case Qt::Key_5: case Qt::Key_6: case Qt::Key_7:
		{
			vmd_time_t size = file()->division * 4;
			for (int i = 0; i < key - Qt::Key_1; i++)
				size /= 2;
			if (mod & Qt::CTRL)
				grid_size_ = size;
			else
				cursor_size_ = size;
		}
		break;
	case Qt::Key_Enter:
	case Qt::Key_Return:
		toggle_note();
		break;
	case Qt::Key_I:
		if (mod == Qt::CTRL) {
			vmd_note_t *note = noteAtCursor();
			if (note == NULL)
				break;
			QPoint pos(time2x(cursor_time_), level2y(cursor_level_));
			QString text = "Note: ";
			text += "channel = "; text += QString::number(note->channel->number);
			QToolTip::hideText();
			QToolTip::showText(mapToGlobal(pos), text);
		}
		break;
	default:
		return QWidget::keyPressEvent(ev);
	}
	look_at_cursor();
	update();
}

static int
clipped(int x, int s, int e)
{
	const int margin = 1;
	if (x < s + margin)
		return s + margin;
	else if (x >= e - margin)
		return e - margin - 1;
	else
		return x;
}

void
WPiano::clipCursor()
{
	if (!mouse_captured_)
		return;
	QRect v = viewport();
	QPoint p1 = mapToGlobal(v.topLeft());
	QPoint p2 = mapToGlobal(v.bottomRight());
	int x = clipped(QCursor::pos().x(), p1.x(), p2.x());
	int y = clipped(QCursor::pos().y(), p1.y(), p2.y());
	QCursor::setPos(QPoint(x, y));
}

void
WPiano::mouseMoveEvent(QMouseEvent *ev)
{
	if (!mouse_captured_)
		return;

	clipCursor();
	setCursorPos(grid_snap_left(this, x2time(ev->x())), y2level(ev->y()));
	look_at_cursor(MINSCROLL);
}

void
WPiano::mousePressEvent(QMouseEvent *ev)
{
	switch (ev->button()) {
	case Qt::LeftButton:
		if (!mouse_captured_) {
			capture_mouse();
			mouseMoveEvent(ev);
		} else
			toggle_note();
		break;
	default:
		;
	}
}

static void
draw_measure(const vmd_measure_t *measure, void *_painter)
{
	QPainter *painter = static_cast<QPainter *>(_painter);
	WPiano *piano = static_cast<WPiano *>(painter->device());
	vmd_time_t cell_size = piano->grid_size() > 0 ? piano->grid_size() : measure->part_size;

	painter->setPen(piano->palette().windowText().color());
	for (vmd_time_t t = measure->beg; t < measure->end; t += cell_size) {
		int x = piano->time2x(t);
		painter->drawLine(x, 0, x, piano->height());
		painter->setPen(piano->palette().mid().color());
	}
}

static void *
draw_note(vmd_note_t *note, void *_painter)
{
	QPainter *painter = static_cast<QPainter *>(_painter);
	WPiano *piano = static_cast<WPiano *>(painter->device());

	int level = pitch2level(piano->track(), note->pitch);
	int y = piano->level2y(level);
	int x1 = piano->time2x(note->on_time);
	int x2 = piano->time2x(note->off_time);
	painter->drawLine(x1, y, x2 - 1, y);
	return NULL;
}

enum {
	LEVEL_NORMAL,
	LEVEL_LINE,
	LEVEL_OCTAVE_LINE
};

static int
level_style(const vmd_track_t *track, int level)
{
	int l = level % (vmd_notesystem_levels(&track->notesystem) + 1);
	if (l == 0)
		return LEVEL_OCTAVE_LINE;
	else if (l % 2 == 0)
		return LEVEL_LINE;
	else
		return LEVEL_NORMAL;
};

void
WPiano::paintEvent(QPaintEvent *ev)
{
	QPainter painter(this);
	QPen pen;
	vmd_time_t beg = x2time(ev->rect().left());
	vmd_time_t end = x2time(ev->rect().right());

	/* vertical grid */
	vmd_file_measures(file(), beg, end + 1, draw_measure, &painter);

	/* horizontal grid */
	for (int i = 0; i < levels(track()); i++) {
		int style = level_style(track(), i);
		int y = level2y(i);
		if (style != LEVEL_NORMAL) {
			pen.setWidth(style == LEVEL_OCTAVE_LINE ? 2 : 0);
			painter.setPen(pen);
			painter.drawLine(0, y, width(), y);
		}
	}

	/* notes */
	pen.setWidth(level_height - 1);
	pen.setCapStyle(Qt::FlatCap);
	painter.setPen(pen);
	vmd_track_range(track(), beg, end, draw_note, &painter);

	/* cursor */
	if (playing()) {
		painter.setPen(QPen());
		int x = time2x(cursor_time_);
		painter.drawLine(x, 0, x, height());
	} else {
		Qt::GlobalColor colors[2] = {
			cursor_pitch() >= 0 ? Qt::red : Qt::lightGray,
			Qt::black
		};
		QRect r = cursor_rect();
		for (int i = 0; i < 2; i++) {
			r.adjust(-1, -1, 1, 1);
			painter.setPen(QColor(colors[i]));
			painter.drawRect(r);
		}
	}
}

void
WPiano::timerEvent(QTimerEvent *ev)
{
	if (ev->timerId() == update_timer_.timerId()) {
		if (playing() && cursor_time_ != player_->time()) {
			vmd_time_t prev_time = cursor_time_;
			setCursorTime(player_->time());
			if (l_time() <= prev_time && prev_time <= r_time())
				look_at_cursor();
		} else if (mouse_captured_)
			clipCursor();
	}
}

void
WPiano::capture_mouse(bool capture)
{
	if (capture == mouse_captured_)
		return;
	if (capture && playing())
		return;

	if (capture) {
#ifdef Q_WS_WIN
		QRect v = viewport();
		QPoint p1 = mapToGlobal(v.topLeft());
		QPoint p2 = mapToGlobal(v.bottomRight());
		RECT rect = {p1.x(), p1.y(), p2.x(), p2.y()};
		ClipCursor(&rect);

		// be sure that cursor doesn't reach the clip boundary
		update_timer_.start(update_freq, this);
#else
		grabMouse();
#endif
		setCursor(Qt::BlankCursor);
	} else {
#ifdef Q_WS_WIN
		ClipCursor(NULL);
		update_timer_.stop();
#else
		releaseMouse();
#endif
		setCursor(Qt::ArrowCursor);
	}
	mouse_captured_ = capture;
	setMouseTracking(mouse_captured_);
}

static int
scroll_snap(WPiano *piano, int x, bool right = false)
{
	if (x < 0)
		return 0;
	vmd_measure_t measure;
	vmd_file_measure_at(piano->file(), piano->x2time(x), &measure);
	return piano->time2x(right ? measure.end : measure.beg);
}

void
WPiano::look_at_cursor(LookMode mode)
{
	QWidget *viewport_widget = qobject_cast<QWidget *>(parent());
	if (viewport_widget == NULL)
		return;
	QScrollArea *scroll = qobject_cast<QScrollArea *>(viewport_widget->parent());
	if (scroll == NULL)
		return;
	QScrollBar *hs = scroll->horizontalScrollBar();
	QScrollBar *vs = scroll->verticalScrollBar();

	QRect v = viewport();
	int x1, x2, y1, y2;
	x1 = time2x(cursor_time());
	if (playing()) {
		x2 = x1 + 1;
		y1 = y2 = (v.top() + v.bottom()) / 2;
	} else {
		x2 = time2x(cursor_time_ + cursor_size());
		y1 = level2y(cursor_level_ + 1);
		y2 = level2y(cursor_level_ - 1);
	}

	switch (mode) {
	case PAGE:
		if (x1 < v.left())
			hs->setValue(scroll_snap(this, x2 - v.width(), true));
		if (x2 > v.right())
			hs->setValue(scroll_snap(this, x1 - 1));
		if (y1 < v.top())
			vs->setValue(y2 - v.height() + level_height);
		if (y2 > v.bottom())
			vs->setValue(y1 - level_height);
		break;
	case MINSCROLL:
		scroll->ensureVisible(x1, y1, level_height, level_height);
		scroll->ensureVisible(x2, y2, level_height, level_height);
		break;
	case CENTER:
		hs->setValue((x1 + x2 - v.width()) / 2);
		vs->setValue((y1 + y2 - v.height()) / 2);
		break;
	}
}

void
WPiano::adjust_y()
{
	cursor_level_ = avg_level(track(), l_time(), r_time());
	look_at_cursor(CENTER);
}

void
WPiano::toggle_note()
{
	vmd_pitch_t p = cursor_pitch();
	if (p < 0)
		return;
	int erased = vmd_track_erase_range(track(), cursor_l(), cursor_r(), p, p + 1);
	if (erased <= 0) {
		vmd_track_insert(track(), cursor_l(), cursor_r(), p);
		file()->commit("Insert Note");
	} else if (erased == 1) {
		file()->commit("Erase Note");
	} else
		file()->commit("Erase Notes");
}

bool
WPiano::playing() const
{
	return player_->file() == file();
}

void
WPiano::playStarted()
{
	if (playing()) {
		capture_mouse(false);
		update_timer_.start(update_freq, this);
	}
}

void
WPiano::playStopped()
{
	update_timer_.stop();
	setCursorTime(grid_snap_left(this, cursor_time_));
	update();
}
