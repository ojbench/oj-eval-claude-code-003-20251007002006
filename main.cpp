#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <queue>
using namespace std;

struct Submission {
    string problem;
    string status;
    int time;
    Submission(string p, string s, int t) : problem(p), status(s), time(t) {}
};

struct ProblemStatus {
    int wrong_before = 0;
    bool solved = false;
    int solved_time = -1;
    int submissions_after_freeze = 0;
    bool is_frozen = false;
};

struct Team {
    string name;
    map<char, ProblemStatus> problems;
    vector<Submission> submissions;
    int penalty_time = 0;
    int solved_count = 0;

    Team(string n) : name(n) {}

    void update_ranking() {
        solved_count = 0;
        penalty_time = 0;
        vector<int> solved_times;

        for (auto& [prob, status] : problems) {
            if (status.solved && !status.is_frozen) {
                solved_count++;
                int penalty = 20 * status.wrong_before + status.solved_time;
                penalty_time += penalty;
                solved_times.push_back(status.solved_time);
            }
        }
        sort(solved_times.rbegin(), solved_times.rend());
    }
};

struct RankingComparator {
    bool operator()(const Team* a, const Team* b) const {
        if (a->solved_count != b->solved_count) {
            return a->solved_count > b->solved_count;
        }
        if (a->penalty_time != b->penalty_time) {
            return a->penalty_time < b->penalty_time;
        }

        vector<int> a_times, b_times;
        for (auto& [prob, status] : a->problems) {
            if (status.solved && !status.is_frozen) {
                a_times.push_back(status.solved_time);
            }
        }
        for (auto& [prob, status] : b->problems) {
            if (status.solved && !status.is_frozen) {
                b_times.push_back(status.solved_time);
            }
        }
        sort(a_times.rbegin(), a_times.rend());
        sort(b_times.rbegin(), b_times.rend());

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
    map<string, Team*> teams;
    vector<Team*> team_list;
    set<Team*, RankingComparator> current_ranking;
    bool competition_started = false;
    bool competition_ended = false;
    int duration_time = 0;
    int problem_count = 0;
    bool is_frozen = false;
    int freeze_time = -1;
    bool scoreboard_flushed = false;
    set<Team*, RankingComparator> last_flushed_ranking;

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
        Team* team = new Team(team_name);
        teams[team_name] = team;
        team_list.push_back(team);
        current_ranking.insert(team);
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
            for (char c = 'A'; c < 'A' + problem_count; c++) {
                team->problems[c] = ProblemStatus();
            }
        }

        cout << "[Info]Competition starts.\n";
    }

    void submit(const string& problem, const string& team_name,
                const string& status, int time) {
        if (!competition_started || competition_ended) return;

        Team* team = teams[team_name];
        team->submissions.emplace_back(problem, status, time);

        char prob_char = problem[0];
        ProblemStatus& prob_status = team->problems[prob_char];

        if (is_frozen && !prob_status.solved) {
            prob_status.submissions_after_freeze++;
            if (!prob_status.is_frozen) {
                prob_status.is_frozen = true;
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
        current_ranking.clear();

        for (auto team : team_list) {
            team->update_ranking();
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
        freeze_time = 0;

        for (auto team : team_list) {
            for (char c = 'A'; c < 'A' + problem_count; c++) {
                if (!team->problems[c].solved) {
                    team->problems[c].is_frozen = true;
                }
            }
        }

        cout << "[Info]Freeze scoreboard.\n";
    }

    void scroll_scoreboard() {
        if (!competition_started || competition_ended) return;

        if (!is_frozen) {
            cout << "[Error]Scroll failed: scoreboard has not been frozen.\n";
            return;
        }

        cout << "[Info]Scroll scoreboard.\n";

        flush_scoreboard();
        print_scoreboard();

        vector<pair<string, string>> changes;
        bool has_changes = true;

        while (has_changes) {
            has_changes = false;

            for (auto it = current_ranking.rbegin(); it != current_ranking.rend(); ++it) {
                Team* team = *it;
                char smallest_frozen = '\0';

                for (char c = 'A'; c < 'A' + problem_count; c++) {
                    if (team->problems[c].is_frozen) {
                        if (smallest_frozen == '\0' || c < smallest_frozen) {
                            smallest_frozen = c;
                        }
                    }
                }

                if (smallest_frozen != '\0') {
                    ProblemStatus& prob_status = team->problems[smallest_frozen];

                    bool was_solved_during_freeze = false;
                    for (const auto& sub : team->submissions) {
                        if (sub.problem == string(1, smallest_frozen) &&
                            sub.time >= freeze_time &&
                            sub.status == "Accepted") {
                            was_solved_during_freeze = true;
                            break;
                        }
                    }

                    if (was_solved_during_freeze) {
                        int solved_time = -1;
                        for (const auto& sub : team->submissions) {
                            if (sub.problem == string(1, smallest_frozen) &&
                                sub.status == "Accepted") {
                                solved_time = sub.time;
                            }
                        }
                        prob_status.solved = true;
                        prob_status.solved_time = solved_time;
                    }

                    prob_status.is_frozen = false;

                    auto old_ranking = current_ranking;
                    current_ranking.clear();
                    for (auto t : team_list) {
                        t->update_ranking();
                        current_ranking.insert(t);
                    }

                    auto old_it = old_ranking.find(team);
                    int old_rank = distance(old_ranking.begin(), old_it) + 1;
                    auto new_it = current_ranking.find(team);
                    int new_rank = distance(current_ranking.begin(), new_it) + 1;

                    if (old_rank != new_rank) {
                        auto team_above = *next(current_ranking.rbegin(), old_rank - 1);
                        changes.push_back({team->name, team_above->name});
                        has_changes = true;
                    }

                    break;
                }
            }
        }

        for (const auto& change : changes) {
            auto team1 = teams[change.first];
            cout << team1->name << " " << change.second << " "
                 << team1->solved_count << " " << team1->penalty_time << "\n";
        }

        is_frozen = false;
        print_scoreboard();
    }

    void print_scoreboard() {
        int rank = 1;
        for (auto team : current_ranking) {
            cout << team->name << " " << rank << " "
                 << team->solved_count << " " << team->penalty_time;

            for (char c = 'A'; c < 'A' + problem_count; c++) {
                const ProblemStatus& status = team->problems[c];
                if (status.is_frozen) {
                    cout << " " << status.wrong_before << "/" << status.submissions_after_freeze;
                } else if (status.solved) {
                    cout << " +" << (status.wrong_before == 0 ? "" : to_string(status.wrong_before));
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
        if (teams.find(team_name) == teams.end()) {
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
        if (teams.find(team_name) == teams.end()) {
            cout << "[Error]Query submission failed: cannot find the team.\n";
            return;
        }

        cout << "[Info]Complete query submission.\n";

        Team* team = teams[team_name];
        const Submission* result = nullptr;

        for (const auto& sub : team->submissions) {
            bool problem_match = (problem == "ALL" || sub.problem == problem);
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
            string team_name, where, problem_str, and_str, status_str;
            string problem, status;
            iss >> team_name >> where >> problem_str >> and_str >> status_str;

            problem = problem_str.substr(9); // Remove "PROBLEM="
            status = status_str.substr(7);   // Remove "STATUS="

            system.query_submission(team_name, problem, status);
        } else if (command == "END") {
            system.end_competition();
            break;
        }
    }

    return 0;
}