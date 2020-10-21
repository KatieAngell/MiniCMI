/*******************************************************************************
 * This file is part of CMacIonize
 * Copyright (C) 2020 Bert Vandenbroucke (bert.vandenbroucke@gmail.com)
 *
 * CMacIonize is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * CMacIonize is distributed in the hope that it will be useful,
 * but WITOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with CMacIonize. If not, see <http://www.gnu.org/licenses/>.
 ******************************************************************************/

/**
 * @file Scheduler.hpp
 *
 * @brief Task scheduler responsible for scheduling and retrieving tasks.
 *
 * @author Bert Vandenbroucke (bert.vandenbroucke@ugent.be)
 */
#ifndef SCHEDULER_HPP
#define SCHEDULER_HPP

#include "TaskQueue.hpp"
#include "ThreadSafeVector.hpp"
#include "Utilities.hpp"

#include <sstream>
#include <vector>

/**
 * @brief Task scheduler responsible for scheduling and retrieving tasks.
 */
class Scheduler {
private:
  /*! @brief Task space. */
  ThreadSafeVector<Task> *_tasks;

  /*! @brief Queues per thread. */
  std::vector<TaskQueue *> _queues;

  /*! @brief General shared queue. */
  TaskQueue *_shared_queue;

public:
  /**
   * @brief Constructor.
   *
   * @param number_of_tasks Number of tasks to allocate space for.
   * @param number_of_queues Number of per thread queues to allocate (it is a
   * good idea to make use the number of threads that are being used).
   * @param queue_size_per_thread Size of the per-thread queue (can usually be
   * small).
   * @param shared_queue_size Size of the single queue that is shared among all
   * threads (should be fairly large).
   */
  inline Scheduler(const size_t number_of_tasks, const size_t number_of_queues,
                   const size_t queue_size_per_thread,
                   const size_t shared_queue_size) {
    _tasks = new ThreadSafeVector<Task>(number_of_tasks, "Tasks");
    _queues.resize(number_of_queues);
    for (size_t i = 0; i < number_of_queues; ++i) {
      std::stringstream queue_name;
      queue_name << "Queue for Thread " << i;
      _queues[i] = new TaskQueue(queue_size_per_thread, queue_name.str());
    }
    _shared_queue = new TaskQueue(shared_queue_size, "Shared queue");
  }

  /**
   * @brief Destructor.
   */
  inline ~Scheduler() {
    delete _tasks;
    for (size_t i = 0; i < _queues.size(); ++i) {
      delete _queues[i];
    }
    delete _shared_queue;
  }

  /**
   * @brief Get a task from one of the queues.
   *
   * @param thread_id Calling thread.
   * @return Index of a locked task that is ready for execution, or NO_TASK if
   * no eligible task could be found.
   */
  inline uint_fast32_t get_task(const int_fast8_t thread_id) {

    uint_fast32_t task_index = _queues[thread_id]->get_task(*_tasks);
    if (task_index == NO_TASK) {

      // try to steal a task from another thread's queue

      // sort the queues by size
      std::vector<size_t> queue_sizes(_queues.size(), 0);
      for (size_t i = 0; i < _queues.size(); ++i) {
        queue_sizes[i] = _queues[i]->size();
      }
      std::vector<uint_fast32_t> sorti = Utilities::argsort(queue_sizes);

      // now try to steal from the largest queue first
      uint_fast32_t i = 0;
      while (task_index == NO_TASK && i < queue_sizes.size() &&
             queue_sizes[sorti[queue_sizes.size() - i - 1]] > 0) {
        task_index =
            _queues[sorti[queue_sizes.size() - i - 1]]->try_get_task(*_tasks);
        ++i;
      }
      if (task_index == NO_TASK) {
        // get a task from the shared queue
        task_index = _shared_queue->get_task(*_tasks);
      }
    }

    return task_index;
  }
};

#endif // SCHEDULER_HPP
