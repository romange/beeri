%x string string_sq
%%
[ \t\n]+                            // skip white space chars.
[0-9]+                         return plang::ParserBase::NUMBER;
"AND"|"and"|"&&"               return plang::ParserBase::AND_OP;
"OR"|"or"|"||"                 return plang::ParserBase::OR_OP;
"<="                           return plang::ParserBase::LE_OP;
">="                           return plang::ParserBase::GE_OP;
"not"|"NOT"                    return plang::ParserBase::NOT_OP;
"!="                           return plang::ParserBase::NE_OP;
"True"|"true"|"TRUE"           {
                                 setMatched("1");
                                 return plang::ParserBase::NUMBER;
                                }
"False"|"false"|"FALSE"        {
                                 setMatched("0");
                                 return plang::ParserBase::NUMBER;
                               }
"def"|"DEF"                    {
                                 return plang::ParserBase::DEF_TOK;
                               }
[[:alpha:]_][[:alnum:]_.]*      return plang::ParserBase::IDENTIFIER;
\"              {
                    begin(StartCondition__::string);
                }
<string>{
    \"          {
                    begin(StartCondition__::INITIAL);
                    setMatched(matched().substr(0, matched().size() -1));
                    // std::cout << "matched: " << matched() << '\n';
                    return plang::ParserBase::STRING;
                }
    \\.|.       {
                    // \\. Is longer than \" so flex will prefer matching it than ending the string.
                    more();
                }
}
\'              {
                    begin(StartCondition__::string_sq);
                }
<string_sq>{
    \'          {
                    begin(StartCondition__::INITIAL);
                    setMatched(matched().substr(0, matched().size() -1));
                    return plang::ParserBase::STRING;
                }
    \\.|.       more();
}
.                              return matched()[0];

