#include <iostream>

using namespace std;

int main() {
    string s;
    cin >> s;
    int count = 0;
    int oldSize = s.size() - 1;
    for (int i = 0; i < s.size(); i++) {
        if (s[i] >= '0' && s[i] <= '9')
            count++;
    }
    s.resize(s.size() + 5 * count);
    int newSize = s.size() - 1;
    for (int i = oldSize; i >= 0; i--) {
        if (s[i] >= '0' && s[i] <= '9') {
            s[newSize--] = 'r';
            s[newSize--] = 'e';
            s[newSize--] = 'b';
            s[newSize--] = 'm';
            s[newSize--] = 'u';
            s[newSize--] = 'n';
        } else {
            s[newSize--] = s[i];
        }
    }
    cout << s;
}