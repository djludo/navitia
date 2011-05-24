#include "ptreferential.h"

#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/qi_lit.hpp>

#include <boost/fusion/include/adapt_struct.hpp>
#include <iostream>
#include "indexes2.h"
#include "where.h"
#include "reflexion.h"

namespace qi = boost::spirit::qi;
using namespace navitia::type;
struct Column {
    std::string table;
    std::string column;
};

BOOST_FUSION_ADAPT_STRUCT(
    Column,
    (std::string, table)
    (std::string, column)
)

struct WhereClause{
    Column col;
    std::string op;
    std::string value;
}; 

BOOST_FUSION_ADAPT_STRUCT(
    WhereClause,
    (Column,col)
    (std::string, op)
    (std::string, value)
)

struct Request {
    std::vector<Column> columns;
    std::vector<std::string> tables;
    std::vector<WhereClause> clauses;
};

BOOST_FUSION_ADAPT_STRUCT(
    Request,
    (std::vector<Column>, columns)
    (std::vector<std::string>, tables)
    (std::vector<WhereClause>, clauses)
)
        template <typename Iterator>
        struct select_r
            : qi::grammar<Iterator, Request(), qi::space_type>
{
    qi::rule<Iterator, std::string(), qi::space_type> txt; // Match une string
    qi::rule<Iterator, Column(), qi::space_type> table_col; // Match une colonne
    qi::rule<Iterator, std::vector<Column>(), qi::space_type> select; // Match toute la section SELECT
    qi::rule<Iterator, std::vector<std::string>(), qi::space_type> from; // Matche la section FROM
    qi::rule<Iterator, Request(), qi::space_type> request; // Toute la requête
    qi::rule<Iterator, std::vector<WhereClause>(), qi::space_type> where;// section Where 
    qi::rule<Iterator, std::string(), qi::space_type> bin_op; // Match une operator binaire telle que <, =...

    select_r() : select_r::base_type(request) {
        txt %= qi::lexeme[+(qi::alnum|'_')]; // Match du texte
        bin_op %=  qi::string("<=") | qi::string(">=") | qi::string("<>") | qi::string("<")  | qi::string(">") | qi::string("=") ;
        table_col %= -(txt >> '.') // (Nom de la table suivit de point) optionnel (operateur -)
                     >> txt; // Nom de la table obligatoire
        select  %= qi::lexeme["select"] >> table_col % ',' ; // La liste de table_col séparée par des ,
        from %= qi::lexeme["from"] >> txt % ',';
        where %= qi::lexeme["where"] >> (table_col >> bin_op >> txt) % qi::lexeme["and"];
        request %= select >> from >> -where;
    }

};

template<class T>
WhereWrapper<T> build_clause(std::vector<WhereClause> clauses) {
    WhereWrapper<T> wh(new BaseWhere<T>());
    Operator_e op;
    BOOST_FOREACH(auto clause, clauses) {
        if(clause.op == "=") op = EQ;
        else if(clause.op == "<") op = LT;
        else if(clause.op == "<=") op = LEQ;
        else if(clause.op == ">") op = GT;
        else if(clause.op == ">=") op = GEQ;
        else if(clause.op == "<>") op = NEQ;
        else throw "grrr";

        if(clause.col.column == "id")
            wh = wh && WHERE(ptr_id<T>(), op, clause.value);
        else if(clause.col.column == "idx")
            wh = wh && WHERE(ptr_idx<T>(), op, clause.value);
        else if(clause.col.column == "external_code")
            wh = wh && WHERE(ptr_external_code<T>(), op, clause.value);
    }
    return wh;
}

template<class T>
std::vector< std::vector<col_t> > extract_data( std::vector<T> & rows, const Request & r) {
    std::vector< std::vector<col_t> > result;
    Index2<boost::fusion::vector<T> > filtered(rows, build_clause<T>(r.clauses));
    BOOST_FOREACH(auto item, filtered){
        std::vector<col_t> row;
        BOOST_FOREACH(const Column & col, r.columns)
           row.push_back(get_value(*(boost::fusion::at_c<0>(item)), col.column));
        result.push_back(row);
    }
    return result;
}

struct unknown_table{};
std::vector< std::vector<col_t> > query(std::string request, Data & data){
    std::vector< std::vector<col_t> > result;
    std::string::iterator begin = request.begin();
    Request r;
    select_r<std::string::iterator> s;
    if (qi::phrase_parse(begin, request.end(), s, qi::space, r))
    {
        if(begin != request.end()) {
            std::cout << "Hrrrmmm on a pas tout parsé -_-'" << std::endl;
        }
    }
    else
        std::cout << "Parsage a échoué" << std::endl;
    std::cout << "Columns : ";
    BOOST_FOREACH(Column & col, r.columns)
            std::cout << col.table << ":" << col.column << " ";
    std::cout << std::endl;

    std::cout << "Clauses where : ";
    BOOST_FOREACH(WhereClause & w, r.clauses)
            std::cout << w.col.column << w.op << w.value << " ";
    std::cout << std::endl;

    if(r.tables.size() != 1){
        std::cout << "Pour l'instant on ne supporte que exactement une table" << std::endl;
        return result;
    }
    else {
        std::cout << "Table : " << r.tables[0] << std::endl;
    }

    std::string table = r.tables[0];

    if(table == "validity_pattern") {
        return extract_data(data.validity_patterns, r);
    }
    else if(table == "lines") {
        return extract_data(data.lines, r);
    }
    else if(table == "routes") {
        return extract_data(data.routes, r);
    }
    else if(table == "vehicle_journey") {
        return extract_data(data.vehicle_journeys, r);
    }
    else if(table == "stop_points") {
        return extract_data(data.stop_points, r);
    }
    else if(table == "stop_areas") {
        return extract_data(data.stop_areas, r);
    }
    else if(table == "stop_times"){
        return extract_data(data.stop_times, r);
    }
    
    throw unknown_table();
}


int main(int argc, char** argv){
    Data d;
    d.load_bin("data.nav");
    Index2<boost::fusion::vector<StopArea> > x2(d.stop_areas, WHERE(ptr_external_code<StopArea>(), EQ, "DUA5500109")); //&& WHERE(Members<StopArea>::get<external_code>(), EQ, "DUANAV|617003|2399936"));
    std::cout << x2.nb_types() << " " << x2.size() << std::endl;

    BOOST_FOREACH(auto bleh, x2) {
        std::cout << boost::fusion::at_c<0>(bleh)->name << " "
                  << boost::fusion::at_c<0>(bleh)->external_code << " "
                  << get_value(*boost::fusion::at_c<0>(bleh), "external_code")
                  << std::endl;
    }

    if(argc != 2)
        std::cout << "Il faut exactement un paramètre" << std::endl;
    else {
        auto result = query(argv[1], d);
        std::cout << "Il y a " << result.size() << " lignes" << std::endl;
        BOOST_FOREACH(auto row, result){
            BOOST_FOREACH(auto col, row){
                std::cout << col << ",\t";
            }
            std::cout << std::endl;
        }
    }
    return 0;
}
