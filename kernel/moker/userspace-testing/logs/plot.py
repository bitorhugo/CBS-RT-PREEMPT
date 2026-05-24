import csv
import sys
import matplotlib.pyplot as plt
from matplotlib.ticker import MaxNLocator, AutoMinorLocator

def parse_logs_and_plot(csv_filename):
    output_img = 'cbs_gantt.png'

    print(f"Reading log file: {csv_filename}...")

    rows = []
    unique_tasks = {}

    # --- PASS 1: Read all data, filter, and identify unique tasks ---
    try:
        with open(csv_filename, 'r') as f:
            reader = csv.reader(f)
            header = next(reader, None)

            for row in reader:
                if len(row) < 8:
                    continue

                # Filter out tasks where task_id is -1
                task_id_str = row[2].strip().strip('*')
                if task_id_str == '-1':
                    continue

                # Convert to integer for proper numerical sorting
                try:
                    task_id = int(task_id_str)
                except ValueError:
                    task_id = task_id_str

                pid = row[6].strip()
                task_name = row[-1].strip()

                rows.append({
                    't_raw': float(row[0]),
                    'event': row[1].strip(),
                    'pid': pid,
                    'task_id': task_id,
                    'task_name': task_name
                })

                if pid not in unique_tasks:
                    unique_tasks[pid] = {
                        'task_id': task_id,
                        'task_name': task_name,
                        'pid': pid
                    }

    except FileNotFoundError:
        print(f"Error: Could not find {csv_filename}.")
        sys.exit(1)

    if not rows:
        print("Warning: No valid tasks found. Exiting.")
        sys.exit(0)

    # --- PASS 2: Sort tasks by task_id and map Y-coordinates ---
    sorted_tasks = sorted(unique_tasks.values(), key=lambda x: (
        x['task_id'] if isinstance(x['task_id'], int) else float('inf'),
        x['pid']
    ))

    tasks_y_map = {}
    for i, task in enumerate(sorted_tasks, start=1):
        tasks_y_map[task['pid']] = i

    current_y = len(sorted_tasks) + 1
    first_timestamp = rows[0]['t_raw']

    print(f"Parsed {len(tasks_y_map)} valid tasks. Generating plot data...")

    # --- PASS 3: Generate the Gantt Data, Event Tracking, and Exec Time ---
    active_tasks = {}
    plot_data = []
    enq_rq_events = []
    replenishment_events = []

    # Initialize execution time tracking dictionary for the Y-axis totals
    task_exec_times = {pid: 0.0 for pid in tasks_y_map}

    for r in rows:
        t = (r['t_raw'] - first_timestamp) / 1_000_000.0
        event = r['event']
        pid = r['pid']

        y_val = tasks_y_map[pid]

        # Start of execution (used for drawing the horizontal bars)
        if event == "SWT_TO":
            active_tasks[pid] = t

        # End of execution
        elif event == "SWT_AY":
            if pid in active_tasks:
                start_time = active_tasks.pop(pid)
                duration = t - start_time

                # Accumulate for the grand total in the Y-axis label
                task_exec_times[pid] += duration

                # Store the block data including PID so we can track alternations per task
                plot_data.append((pid, y_val, start_time, duration))

        # Task Arrival / Ready Event
        elif event == "ENQ_RQ":
            enq_rq_events.append((t, y_val))

        # Look for the specific soft replenishment event
        elif "BUDGET_REPLEN_SOFT" in event.upper():
            replenishment_events.append((t, y_val))

    # --- Generate Y Labels with Total Execution Times ---
    y_ticks = []
    y_labels = []

    print("\n--- Task Execution Summary ---")
    for i, task in enumerate(sorted_tasks, start=1):
        pid = task['pid']
        total_exec = task_exec_times[pid]

        print(f"Task: {task['task_name']:<15} (ID: {task['task_id']}) | Total Exec Time: {total_exec:.3f} ms")

        label = f"{task['task_name']} (ID: {task['task_id']})\n[Total Exec: {total_exec:.2f} ms]"
        y_ticks.append(i)
        y_labels.append(label)
    print("------------------------------\n")

    # --- Generate Plot with Matplotlib ---
    print("Generating plot with Matplotlib...")
    fig, ax = plt.subplots(figsize=(16, 8))

    # Keep track of how many blocks we've drawn per task to alternate text position
    task_block_counters = {pid: 0 for pid in tasks_y_map}

    # Plot each active segment using horizontal bars and add individual duration text
    for pid, y_val, start_time, duration in plot_data:
        # Draw the rectangle
        ax.barh(y_val, duration, left=start_time, height=0.4, color=f'C{y_val % 10}', align='center', zorder=2)

        # Determine text position (Alternate top and bottom for overlapping readability)
        text_x = start_time + (duration / 2)

        if task_block_counters[pid] % 2 == 0:
            # Draw on Top
            text_y = y_val + 0.25 # Slightly above the bar
            va = 'bottom'         # Align the text so it grows upwards from the coordinate
        else:
            # Draw on Bottom
            text_y = y_val - 0.25 # Slightly below the bar
            va = 'top'            # Align the text so it hangs downwards from the coordinate

        task_block_counters[pid] += 1

        # Add the text
        ax.text(text_x, text_y, f"{duration:.2f}",
                ha='center', va=va, fontsize=7, color='black',
                fontweight='bold', zorder=5)

    # Draw upward arrows for every ENQ_RQ event (Task Arrivals)
    if enq_rq_events:
        X = [t for t, y in enq_rq_events]
        Y = [y - 0.25 for t, y in enq_rq_events]
        U = [0] * len(X)
        V = [0.65] * len(X)
        ax.quiver(X, Y, U, V, angles='xy', scale_units='xy', scale=1,
                  color='black', width=0.002, headlength=4, headwidth=4, zorder=3)

    # Draw red vertical lines for Replenishment events
    if replenishment_events:
        for t, y in replenishment_events:
            ax.vlines(x=t, ymin=y - 0.4, ymax=y + 0.4, color='red', linewidth=2.5, zorder=4)

    # Add labels and title
    ax.set_title('CBS Task Scheduling Timeline (Ordered by Task ID)')
    ax.set_xlabel('Time (ms)')
    ax.set_ylabel('Tasks')

    # Apply the y-axis ticks and labels
    ax.set_yticks(y_ticks)
    ax.set_yticklabels(y_labels)
    ax.set_ylim(0.5, max(1, current_y - 0.5))

    # --- X-Axis Granularity Improvements ---
    ax.xaxis.set_major_locator(MaxNLocator(nbins=25))
    ax.xaxis.set_minor_locator(AutoMinorLocator(5))

    ax.grid(True, which='major', axis='x', linestyle='-', alpha=0.4)
    ax.grid(True, which='minor', axis='x', linestyle=':', alpha=0.3)
    ax.set_axisbelow(True)

    # Automatically format X-axis labels if they start overlapping
    plt.setp(ax.get_xticklabels(), rotation=45, ha='right')

    plt.tight_layout()

    # Save to file
    plt.savefig(output_img, dpi=300)
    print(f"Success! Graph saved as {output_img}")

if __name__ == "__main__":
    parse_logs_and_plot("cbs-logs.csv")
