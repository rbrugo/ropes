/**
 * @author      : rbrugo (brugo.riccardo@gmail.com)
 * @file        : expression
 * @created     : Wednesday Aug 14, 2024 21:40:51 CEST
 * @description : 
 */

#include <expression.hpp>
#include <iostream>

int main(int argc, char * argv[])
{
    if (argc < 3) {
        std::cout << "Usage: " << argv[0] << " '<expr>' <name> <value>";
        return 1;
    }
    auto expr = brun::expr::expression(argv[1], argv[2]);
    std::cout << expr.eval(brun::expr::parameter{*argv[2], std::stod(argv[3])});
}
