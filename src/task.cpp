#include "task.h"

#include <ostream>
#include <utility>

Task::Task(std::string text, bool done) : text_(std::move(text)), done_(done) {}

const std::string& Task::text() const { return text_; }

bool Task::isDone() const { return done_; }

void Task::markDone() { done_ = !done_; }

bool Task::operator==(const Task& other) const {
    return text_ == other.text_ && done_ == other.done_;
}

std::ostream& operator<<(std::ostream& output, const Task& task) {
    output << '[' << (task.done_ ? 'x' : ' ') << "] " << task.text_;

    return output;
}