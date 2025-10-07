#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <sstream>
#include <unordered_map>
using namespace std;

struct SubmissionRecord {
    char problem;
    string status;
    int time;
    SubmissionRecord(char p, const string& s, int t) : problem(p), status(s), time(t) {}
};

struct ProblemStatus {
    int wrong_before = 0;
    bool solved = false;
    int solved_time = -1;
    int submissions_after_freeze = 0;
    bool is_frozen = false;
    int freeze_time = -1;
};

struct Team {
    string name;
    vector<ProblemStatus> problems;
    vector<SubmissionRecord> submissions;
    long long penalty_time = 0;
    int solved_count = 0;

    Team(const string& n, int problem_count) : name(n), problems(problem_count) {}

    void calculate_ranking() {
        solved_count = 0;
        penalty_time = 0;

        for (int i = 0; i < (int)problems.size(); i++) {
            const auto& status = problems[i];
            if (status.solved && !status.is_frozen) {
                solved_count++;
                penalty_time += 20LL * status.wrong_before + status.solved_time;
            }
        }
    }

    vector<int> get_solved_times() const {
        vector<int> times;
        for (int i = 0; i < (int)problems.size(); i++) {
            const auto& status = problems[i];
            if (status.solved && !status.is_frozen) {
                times.push_back(status.solved_time);
            }
        }
        sort(times.rbegin(), times.rend());
        return times;
    }
};

struct TeamComparator {
    bool operator()(const Team* a, const Team* b) const {
        if (a->solved_count != b->solved_count) {
            return a->solved_count > b->solved_count;
        }
        if (a->penalty_time != b->penalty_time) {
            return a->penalty_time < b->penalty_time;
        }

        vector<int> a_times = a->get_solved_times();
        vector<int> b_times = b->get_solved_times();

        for (size_t i = 0; i < min(a_times.size(), b_times.size()); i++) {
            if (a_times[i] != b_times[i]) {
                return a_times[i] < b_times[i];
            }
        }
        return a->name < b->name;
    }
};

class ICPCManagement {
private:
    unordered_map<string, Team*> teams;
    vector<Team*> team_list;
    bool competition_started = false;
    bool competition_ended = false;
    int duration_time = 0;
    int problem_count = 0;
    bool is_frozen = false;
    int freeze_time = -1;
    bool scoreboard_flushed = false;
    set<Team*, TeamComparator> last_flushed_ranking;

    void update_all_rankings() {
        for (auto team : team_list) {
            team->calculate_ranking();
        }
    }

public:
    ~ICPCManagement() {
        for (auto& [name, team] : teams) {
            delete team;
        }
    }

    void add_team(const string& team_name) {
        if (competition_started) {
            cout << "[Error]Add failed: competition has started.\n";
            return;
        }
        if (teams.find(team_name) != teams.end()) {
            cout << "[Error]Add failed: duplicated team name.\n";
            return;
        }
        Team* team = new Team(team_name, problem_count);
        teams[team_name] = team;
        team_list.push_back(team);
        cout << "[Info]Add successfully.\n";
    }

    void start_competition(int duration, int problems) {
        if (competition_started) {
            cout << "[Error]Start failed: competition has started.\n";
            return;
        }
        duration_time = duration;
        problem_count = problems;
        competition_started = true;

        for (auto& [name, team] : teams) {
            team->problems.resize(problem_count);
        }

        cout << "[Info]Competition starts.\n";
    }

    void submit(const string& problem, const string& team_name,
                const string& status, int time) {
        if (!competition_started || competition_ended) return;

        auto it = teams.find(team_name);
        if (it == teams.end()) return;

        Team* team = it->second;
        char prob_char = problem[0];
        int prob_index = prob_char - 'A';

        if (prob_index < 0 || prob_index >= problem_count) return;

        team->submissions.emplace_back(prob_char, status, time);
        ProblemStatus& prob_status = team->problems[prob_index];

        if (is_frozen && !prob_status.solved) {
            prob_status.submissions_after_freeze++;
            if (!prob_status.is_frozen) {
                prob_status.is_frozen = true;
                prob_status.freeze_time = time;
            }
        } else if (!is_frozen) {
            if (status == "Accepted" && !prob_status.solved) {
                prob_status.solved = true;
                prob_status.solved_time = time;
            } else if (status != "Accepted" && !prob_status.solved) {
                prob_status.wrong_before++;
            }
        }
    }

    void flush_scoreboard() {
        if (!competition_started || competition_ended) return;

        scoreboard_flushed = true;
        update_all_rankings();

        set<Team*, TeamComparator> current_ranking;
        for (auto team : team_list) {
            current_ranking.insert(team);
        }

        last_flushed_ranking = current_ranking;
        cout << "[Info]Flush scoreboard.\n";
    }

    void freeze_scoreboard() {
        if (!competition_started || competition_ended) return;

        if (is_frozen) {
            cout << "[Error]Freeze failed: scoreboard has been frozen.\n";
            return;
        }

        is_frozen = true;
        flush_scoreboard();

        cout << "[Info]Freeze scoreboard.\n";
    }

    void scroll_scoreboard() {
        if (!competition_started || competition_ended) return;

        if (!is_frozen) {
            cout << "[Error]Scroll failed: scoreboard has not been frozen.\n";
            return;
        }

        cout << "[Info]Scroll scoreboard.\n";

        // First flush scoreboard
        flush_scoreboard();

        // Print scoreboard before scrolling
        print_scoreboard(last_flushed_ranking);

        // Track ranking changes
        vector<tuple<string, string, int, long long>> changes;

        // Create set of teams with their initial rankings
        set<Team*, TeamComparator> current_ranking;
        for (auto team : team_list) {
            current_ranking.insert(team);
        }

        // Process unfreezing
        while (true) {
            // Find lowest-ranked team with frozen problems
            Team* target_team = nullptr;
            char target_problem = '\0';

            auto ranking_vec = vector<Team*>(current_ranking.begin(), current_ranking.end());

            // Search from lowest rank (end of vector) to highest
            for (auto it = ranking_vec.rbegin(); it != ranking_vec.rend(); ++it) {
                Team* team = *it;
                for (int i = 0; i < problem_count; i++) {
                    if (team->problems[i].is_frozen) {
                        target_team = team;
                        target_problem = 'A' + i;
                        break;
                    }
                }
                if (target_team) break;
            }

            if (!target_team) break;

            int prob_index = target_problem - 'A';
            ProblemStatus& prob_status = target_team->problems[prob_index];

            // Check if problem was solved during freeze
            bool solved_during_freeze = false;
            for (const auto& sub : target_team->submissions) {
                if (sub.problem == target_problem && sub.status == "Accepted") {
                    prob_status.solved = true;
                    prob_status.solved_time = sub.time;
                    solved_during_freeze = true;
                    break;
                }
            }

            prob_status.is_frozen = false;

            // Recalculate rankings
            update_all_rankings();
            set<Team*, TeamComparator> new_ranking;
            for (auto team : team_list) {
                new_ranking.insert(team);
            }

            // Check if ranking changed
            auto old_pos = find(ranking_vec.begin(), ranking_vec.end(), target_team);
            int old_rank = distance(ranking_vec.begin(), old_pos) + 1;

            vector<Team*> new_ranking_vec(new_ranking.begin(), new_ranking.end());
            auto new_pos = find(new_ranking_vec.begin(), new_ranking_vec.end(), target_team);
            int new_rank = distance(new_ranking_vec.begin(), new_pos) + 1;

            if (new_rank < old_rank && solved_during_freeze) {
                // Find the team that was replaced
                string replaced_team = ranking_vec[old_rank - 1 - (old_rank - new_rank)]->name;
                changes.emplace_back(target_team->name, replaced_team,
                                    target_team->solved_count, target_team->penalty_time);
            }

            current_ranking = new_ranking;
        }

        // Output ranking changes
        for (const auto& [team1, team2, solved, penalty] : changes) {
            cout << team1 << " " << team2 << " " << solved << " " << penalty << "\n";
        }

        // Print final scoreboard
        print_scoreboard(current_ranking);

        is_frozen = false;
    }

    void print_scoreboard(const set<Team*, TeamComparator>& ranking) {
        int rank = 1;
        for (auto team : ranking) {
            cout << team->name << " " << rank << " "
                 << team->solved_count << " " << team->penalty_time;

            for (int i = 0; i < problem_count; i++) {
                const ProblemStatus& status = team->problems[i];
                if (status.is_frozen) {
                    cout << " " << status.wrong_before << "/" << status.submissions_after_freeze;
                } else if (status.solved) {
                    if (status.wrong_before == 0) {
                        cout << " +";
                    } else {
                        cout << " +" << status.wrong_before;
                    }
                } else {
                    if (status.wrong_before == 0) {
                        cout << " .";
                    } else {
                        cout << " -" << status.wrong_before;
                    }
                }
            }
            cout << "\n";
            rank++;
        }
    }

    void query_ranking(const string& team_name) {
        auto it = teams.find(team_name);
        if (it == teams.end()) {
            cout << "[Error]Query ranking failed: cannot find the team.\n";
            return;
        }

        cout << "[Info]Complete query ranking.\n";
        if (is_frozen) {
            cout << "[Warning]Scoreboard is frozen. The ranking may be inaccurate until it were scrolled.\n";
        }

        if (scoreboard_flushed) {
            int rank = 1;
            for (auto team : last_flushed_ranking) {
                if (team->name == team_name) {
                    cout << "[" << team_name << "] NOW AT RANKING [" << rank << "]\n";
                    return;
                }
                rank++;
            }
        } else {
            vector<Team*> sorted_teams = team_list;
            sort(sorted_teams.begin(), sorted_teams.end(),
                [](const Team* a, const Team* b) {
                    return a->name < b->name;
                });

            for (size_t i = 0; i < sorted_teams.size(); i++) {
                if (sorted_teams[i]->name == team_name) {
                    cout << "[" << team_name << "] NOW AT RANKING [" << (i + 1) << "]\n";
                    return;
                }
            }
        }
    }

    void query_submission(const string& team_name, const string& problem,
                         const string& status) {
        auto it = teams.find(team_name);
        if (it == teams.end()) {
            cout << "[Error]Query submission failed: cannot find the team.\n";
            return;
        }

        cout << "[Info]Complete query submission.\n";

        Team* team = it->second;
        const SubmissionRecord* result = nullptr;

        for (const auto& sub : team->submissions) {
            bool problem_match = (problem == "ALL" || string(1, sub.problem) == problem);
            bool status_match = (status == "ALL" || sub.status == status);

            if (problem_match && status_match) {
                result = &sub;
            }
        }

        if (result == nullptr) {
            cout << "Cannot find any submission.\n";
        } else {
            cout << "[" << team_name << "] [" << result->problem << "] ["
                 << result->status << "] [" << result->time << "]\n";
        }
    }

    void end_competition() {
        if (!competition_started || competition_ended) return;

        competition_ended = true;
        cout << "[Info]Competition ends.\n";
    }
};

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    ICPCManagement system;
    string line;

    while (getline(cin, line)) {
        istringstream iss(line);
        string command;
        iss >> command;

        if (command == "ADDTEAM") {
            string team_name;
            iss >> team_name;
            system.add_team(team_name);
        } else if (command == "START") {
            string duration_str, problem_str;
            int duration, problems;
            iss >> duration_str >> duration >> problem_str >> problems;
            system.start_competition(duration, problems);
        } else if (command == "SUBMIT") {
            string problem, by, team_name, with, status, at, time_str;
            int time;
            iss >> problem >> by >> team_name >> with >> status >> at >> time_str >> time;
            system.submit(problem, team_name, status, time);
        } else if (command == "FLUSH") {
            system.flush_scoreboard();
        } else if (command == "FREEZE") {
            system.freeze_scoreboard();
        } else if (command == "SCROLL") {
            system.scroll_scoreboard();
        } else if (command == "QUERY_RANKING") {
            string team_name;
            iss >> team_name;
            system.query_ranking(team_name);
        } else if (command == "QUERY_SUBMISSION") {
            string team_name, where, problem_part, and_str, status_part;
            iss >> team_name >> where >> problem_part >> and_str >> status_part;

            string problem, status;
            size_t problem_pos = problem_part.find('=');
            size_t status_pos = status_part.find('=');

            if (problem_pos != string::npos) {
                problem = problem_part.substr(problem_pos + 1);
            }
            if (status_pos != string::npos) {
                status = status_part.substr(status_pos + 1);
            }

            system.query_submission(team_name, problem, status);
        } else if (command == "END") {
            system.end_competition();
            break;
        }
    }

    return 0;
}