#include <array>
#include <unordered_map>
#include "../basic/config.h"
#include "../util/io/text_input_file.h"
#include "../util/string/tokenizer.h"
#include "../util/log_stream.h"

using std::cout;
using std::endl;
using std::array;
using std::unordered_multimap;
using std::map;

struct Assoc {
	uint8_t id, fam_idx;
};

static string query_aln;
static vector<array<int, 10>> totals, counts;
static map<string, uint8_t> fam2idx;
static unordered_multimap<string, Assoc> acc2id;
static size_t unmapped_query = 0;

static void fetch_map(TextInputFile& map_in, const string& query) {
	string q, target, family;
	float id;
	acc2id.clear();
	fam2idx.clear();
	counts.clear();
	totals.clear();
	while (map_in.getline(), !map_in.eof()) {
		Util::String::Tokenizer(map_in.line, "\t") >> q >> target >> id >> family;
		if (q != query) {
			if (q < query)
				continue;
			else {
				map_in.putback_line();
				return;
			}
		}
		auto it = fam2idx.emplace(family, (uint8_t)fam2idx.size());
		if (it.second) {
			totals.emplace_back();
			totals.back().fill(0);
			counts.emplace_back();
			counts.back().fill(0);
		}
		const uint8_t fam_idx = it.first->second;
		int bin = std::min((int)(id*100.0) / 10, 9);
		acc2id.emplace(target, Assoc { (uint8_t)bin, fam_idx });
		++totals[(size_t)fam_idx][bin];
	}
}

static void print() {
	if (unmapped_query)
		return;
	cout << query_aln;
	for (int i = 0; i < 10; ++i) {
		double s = 0.0, n = 0.0;
		for (size_t fam_idx = 0; fam_idx < fam2idx.size(); ++fam_idx) {
			if (totals[fam_idx][i] > 0) {
				s += (double)counts[fam_idx][i] / (double)totals[fam_idx][i];
				n += 1.0;
			}
		}
		cout << '\t' << (n > 0.0 ? s / n : -1.0);
	}
	cout << endl;
}

void roc_id() {
	TextInputFile in(config.query_file);
	string query, target;
	size_t n = 0, queries = 0, unmapped = 0, hits = 0;

	TextInputFile map_in(config.family_map);
	
	while (in.getline(), !in.eof()) {
		if (in.line.empty())
			break;
		Util::String::Tokenizer(in.line, "\t") >> query >> target;
		++hits;
		if (query != query_aln) {
			print();
			fetch_map(map_in, query);
			query_aln = query;
			++queries;
			unmapped_query = 0;
			if (queries % 1000 == 0)
				message_stream << queries << ' ' << hits << ' ' << unmapped << endl;
		}

		auto it = acc2id.equal_range(target);
		if (it.first == it.second) {
			++unmapped_query;
			++unmapped;
			continue;
		}
		for (auto i = it.first; i != it.second; ++i)
			++counts[(size_t)i->second.fam_idx][(size_t)i->second.id];
	}
	print();

	in.close();
	map_in.close();
	message_stream << "Queries = " << queries << endl;

}