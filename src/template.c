#include "templator/command_tokenizer.h"
#include "templator/error.h"
#include "templator/parser.h"
#include "templator/template.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define INITIAL_VARIABLES_CAPACITY 10
#define INITIAL_INSTRUCTIONS_CAPACITY 20

int template_is_opening_bracket(char* data, size_t len) {
    return strncmp(data, "{%", (len >= 2) ? 2 : len) == 0;
}

int template_is_closing_bracket(char* data, size_t len) {
    return strncmp(data, "%}", (len >= 2) ? 2 : len) == 0;
}

int template_parse(Template* template, Parser* parser) {
    template->instructions = malloc(INITIAL_INSTRUCTIONS_CAPACITY * sizeof(Instruction));
    template->instructionsCnt = 0;
    template->instructionsCap = INITIAL_INSTRUCTIONS_CAPACITY;
    template->variables = malloc(INITIAL_VARIABLES_CAPACITY * sizeof(char*));
    template->variablesCnt = 0;
    template->variablesCap = INITIAL_VARIABLES_CAPACITY;

    Parser parsed = parser_read_until_str(parser, template_is_opening_bracket, true); 
    while (parsed.data != NULL) {
        if (parsed.len > 0) {
            Instruction* ins = template_add_instruction(template);
            insert_text_instruction_init(ins, parsed.data, parsed.len);
        }
        parser_skip(parser, 2);
        parsed = parser_read_until_str(parser, template_is_closing_bracket, true);
        if (parsed.data == NULL) {
            return TEMPLATE_INCOMPLETE_INSTRUCTION_BRACKETS;
        }
        parser_skip(parser, 2);
        int res = template_parse_instruction(template, parsed, parser);
        if (res < 0) {
            return res;
        }
        parsed = parser_read_until_str(parser, template_is_opening_bracket, true);
    }

    if (parser->len > 0) {
        Instruction* ins = template_add_instruction(template);
        insert_text_instruction_init(ins, parser->data, parser->len);
    }

    return 0;
}

int template_parse_instruction(Template* template, Parser commandParser, Parser* afterCommandParser) {
    Token tok = templator_parser_next_token(&commandParser);
    if (tok.type == NONE) {
        return TEMPLATE_NO_INSTRUCTION_IN_BRACKETS;
    }
    Token nextToken = templator_parser_next_token(&commandParser);
    if (tok.type == KEYWORD_ENDIF && nextToken.type == NONE) {
        return TEMPLATOR_PARSING_ENDED_WITH_ENDIF;
    }


    Instruction* instr = template_add_instruction(template);
    instr->type = NOOP;
    switch (tok.type) {
        case WORD:
            if (nextToken.type != NONE) {
                return TEMPLATOR_UNABLE_TO_PARSE_INSTRUCTION; 
            }
            insert_variable_instruction_init(instr, template_try_insert_variable(template, tok.data, tok.len));
            return 0;
        case KEYWORD_IF:
            if (nextToken.type != PAREN_OPEN) {
                return TEMPLATOR_UNABLE_TO_PARSE_INSTRUCTION;
            }
            ComparisonChain cc;
            int res = comparison_chain_parse(&cc, &commandParser, template);
            if (res < 0) {
                return res;
            }
            Template subTemplate;
            res = template_parse(&subTemplate, afterCommandParser);
            if (res != TEMPLATOR_PARSING_ENDED_WITH_ENDIF) {
                if (res == 0) {
                    return TEMPLATOR_PARSING_DIDNT_END_WITH_ENDIF;
                } else {
                    return res;
                }
            }
            conditional_insert_text_instruction_init(instr, subTemplate, cc);
            return 0;
        default:
            return TEMPLATOR_UNABLE_TO_PARSE_INSTRUCTION;
    }

}

void template_free(Template* template) {
    for (size_t i = 0; i < template->instructionsCnt; ++i) {
        instruction_free(&template->instructions[i]);
    }
    free(template->instructions);
    template->instructions = NULL;
    for (size_t i = 0; i < template->variablesCnt; ++i) {
        free(template->variables[i]);
    }
    free(template->variables);
    template->variables = NULL;
}

Instruction* template_add_instruction(Template* template) {
    if (template->instructionsCnt == template->instructionsCap) {
        template->instructionsCap *= 2;
        template->instructions = realloc(template->instructions, template->instructionsCap * sizeof(Instruction));
    }
    return &template->instructions[template->instructionsCnt++];
}

size_t template_try_insert_variable(Template* template, char* data, size_t len) {
    for (size_t i = 0; i < template->variablesCnt; ++i) {
        if (strncmp(template->variables[i], data, len) == 0) {
            return i;
        }
    }
    if (template->variablesCnt == template->variablesCap) {
        template->variablesCap *= 2;
        template->variables = realloc(template->variables, sizeof(char*) * template->variablesCap);
    }
    template->variables[template->variablesCnt] = malloc(len + 1);
    strncpy(template->variables[template->variablesCnt], data, len);

    template->variables[template->variablesCnt][len] = 0;
    return template->variablesCnt++;
}